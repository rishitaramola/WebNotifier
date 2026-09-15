// ============================================================
// scheduler/src/scheduler.cpp
// Task Scheduler Implementation
// Owner: Rishita Ramola
//
// OS Concepts Demonstrated:
//   - Daemon thread       — scheduler loop runs in background
//   - Exponential backoff — DB reconnect on failure (1s→2s→4s…64s)
//   - Mutex protection    — websites_ list guarded during hot-reload
//   - Pacing / yielding   — PUSH_PACE_MS between enqueues + sleep_for
//   - RAII                — PGconn managed, closed in destructor/stop
//
// Dependency contracts:
//   - Pushes to TaskQueue (Shivank's interface, task.h)
//   - Reads from `websites` table (Divyansh's schema)
//   - Inserts into `monitoring_jobs` to record dispatch
// ============================================================
#include "../include/scheduler.h"
#include <libpq-fe.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <functional>
#include <cstdlib>    // getenv


namespace webnotifier {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper: build connection string from environment variables.
// Never hardcode credentials in source code.
// ─────────────────────────────────────────────────────────────────────────────
static std::string build_conn_string() {
    // Expected env vars (set in configs/.env, loaded by start script):
    //   DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD
    auto env = [](const char* name, const char* dflt = "") -> std::string {
        const char* v = std::getenv(name);
        return v ? v : dflt;
    };
    std::ostringstream oss;
    oss << "host="     << env("DB_HOST",     "localhost")
        << " port="    << env("DB_PORT",     "5432")
        << " dbname="  << env("DB_NAME",     "webnotifier")
        << " user="    << env("DB_USER",     "postgres")
        << " password="<< env("DB_PASSWORD", "")
        << " sslmode=" << env("DB_SSLMODE",  "prefer")
        << " connect_timeout=5";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
Scheduler::Scheduler(TaskQueue& queue,
                     std::shared_ptr<SchedulingStrategy> strategy)
    : queue_(queue),
      strategy_(strategy ? strategy : std::make_shared<FixedIntervalStrategy>()),
      running_(false),
      db_conn_string_(build_conn_string()) {}

Scheduler::~Scheduler() {
    stop();
    if (pg_conn_) {
        PQfinish(static_cast<PGconn*>(pg_conn_));
        pg_conn_ = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public interface
// ─────────────────────────────────────────────────────────────────────────────
void Scheduler::start() {
    if (running_.load()) return;

    std::cout << "[Scheduler] Connecting to database...\n";
    if (!connect_with_retry()) {
        // Continue anyway — the daemon loop will retry each cycle
        std::cerr << "[Scheduler] WARNING: Initial DB connection failed. "
                     "Will retry each tick.\n";
    }

    running_.store(true);
    reload_websites();
    daemon_thread_ = std::thread(&Scheduler::run, this);
    std::cout << "[Scheduler] Daemon started with "
              << websites_.size() << " website(s).\n";
}

void Scheduler::stop() {
    if (!running_.exchange(false)) return;  // already stopped
    if (daemon_thread_.joinable()) {
        daemon_thread_.join();
    }
    std::cout << "[Scheduler] Daemon stopped.\n";
}

bool Scheduler::is_running() const { return running_.load(); }

void Scheduler::add_website(const WebsiteConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    websites_.push_back(config);
}

void Scheduler::reload_websites() {
    auto configs = load_from_db();
    std::lock_guard<std::mutex> lock(mutex_);
    websites_ = std::move(configs);
    std::cout << "[Scheduler] Loaded " << websites_.size() << " website(s) from DB.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Main daemon loop
//
// OS Concept: Tick-based scheduling algorithm
//   Every TICK_INTERVAL_SEC (60 s):
//     1. Record current epoch-time (OS clock).
//     2. For each active website, ask the strategy if it is due.
//     3. If due, create a Task and push() it to the TaskQueue.
//     4. Pace pushes by sleeping PUSH_PACE_MS between each to avoid
//        bursting the queue — simulates OS-level task pacing.
//     5. Sleep for TICK_INTERVAL_SEC before next cycle.
//     6. Every 10 ticks, reload websites from DB (hot-reload).
// ─────────────────────────────────────────────────────────────────────────────
void Scheduler::run() {
    int tick_count = 0;

    while (running_.load()) {
        // ── Reconnect if connection was lost ──────────────────
        if (!pg_conn_ ||
            PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK) {
            std::cerr << "[Scheduler] DB connection lost. Reconnecting...\n";
            connect_with_retry();  // best-effort; loop continues even if fails
        }

        // ── Hot-reload every 10 ticks (≈10 minutes) ──────────
        if (tick_count % 10 == 0 && tick_count > 0) {
            reload_websites();
        }
        ++tick_count;

        // ── Determine current epoch time ──────────────────────
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

        // ── Dispatch due tasks (under lock) ───────────────────
        std::vector<WebsiteConfig> due_sites;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& site : websites_) {
                if (!site.is_active) continue;
                long long last = last_checked_.count(site.website_id)
                                     ? last_checked_[site.website_id] : 0LL;
                if (strategy_->should_check(site.website_id, last,
                                            site.check_interval_min, now)) {
                    due_sites.push_back(site);
                    last_checked_[site.website_id] = now;
                }
            }
        }

        // ── Push tasks outside the lock (queue has its own mutex) ─
        for (const auto& site : due_sites) {
            // Insert a monitoring_job row to get an official job_id
            int job_id = insert_monitoring_job(site.website_id);

            Task task(job_id, site.website_id, site.url,
                      site.keyword, site.timeout_sec, site.notify_email);

            // ISO8601 scheduled_at timestamp
            auto tp   = std::chrono::system_clock::now();
            auto tt   = std::chrono::system_clock::to_time_t(tp);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ",
                          std::gmtime(&tt));
            task.scheduled_at = buf;

            queue_.push(task);
            std::cout << "[Scheduler] Queued job_id=" << job_id
                      << " url=" << site.url << "\n";

            // OS Concept: Pacing — yield between pushes to
            // prevent instantaneous queue flood on many sites.
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PUSH_PACE_MS));
        }

        // ── Sleep until next tick ─────────────────────────────
        // Interruptible loop: check running_ each second so stop()
        // is responsive without blocking for a full 60-second sleep.
        for (int s = 0; s < TICK_INTERVAL_SEC && running_.load(); ++s) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Connect to PostgreSQL with exponential backoff
//
// OS Concept: Exponential backoff — prevents thundering herd on DB restart.
//   Retry delay sequence: 1s, 2s, 4s, 8s, 16s, 32s, 64s (capped).
// ─────────────────────────────────────────────────────────────────────────────
bool Scheduler::connect_with_retry() {
    // Close any stale connection first (RAII-style cleanup)
    if (pg_conn_) {
        PQfinish(static_cast<PGconn*>(pg_conn_));
        pg_conn_ = nullptr;
    }

    int delay_sec = 1;
    for (int attempt = 1; attempt <= MAX_DB_RETRIES; ++attempt) {
        PGconn* conn = PQconnectdb(db_conn_string_.c_str());
        if (PQstatus(conn) == CONNECTION_OK) {
            pg_conn_ = conn;
            std::cout << "[Scheduler] DB connected (attempt " << attempt << ").\n";
            return true;
        }
        std::cerr << "[Scheduler] DB connect attempt " << attempt
                  << " failed: " << PQerrorMessage(conn)
                  << " — retrying in " << delay_sec << "s...\n";
        PQfinish(conn);

        // Sleep while respecting the running_ flag for clean shutdown
        for (int s = 0; s < delay_sec && running_.load(); ++s) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running_.load()) return false;  // stop() was called mid-retry

        delay_sec = std::min(delay_sec * 2, MAX_BACKOFF_SEC);
    }
    std::cerr << "[Scheduler] Giving up DB connection after "
              << MAX_DB_RETRIES << " retries.\n";
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Load active websites from PostgreSQL
//
// DBMS Concept: SELECT with WHERE filter on is_active.
// Returns empty vector (not a crash) if DB is unavailable.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<WebsiteConfig> Scheduler::load_from_db() {
    std::vector<WebsiteConfig> configs;

    if (!pg_conn_ ||
        PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK) {
        std::cerr << "[Scheduler] load_from_db: no DB connection — "
                     "skipping reload.\n";
        return configs;
    }

    const char* sql =
        "SELECT id, url, COALESCE(keyword,''), timeout_seconds, "
        "       COALESCE(notify_email,''), check_interval_min, is_active "
        "FROM   websites "
        "WHERE  is_active = TRUE "
        "ORDER  BY id";

    PGresult* res = PQexec(static_cast<PGconn*>(pg_conn_), sql);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::cerr << "[Scheduler] load_from_db query error: "
                  << PQerrorMessage(static_cast<PGconn*>(pg_conn_)) << "\n";
        PQclear(res);
        return configs;
    }

    int nrows = PQntuples(res);
    configs.reserve(static_cast<size_t>(nrows));

    for (int r = 0; r < nrows; ++r) {
        WebsiteConfig cfg;
        cfg.website_id        = std::stoi(PQgetvalue(res, r, 0));
        cfg.url               = PQgetvalue(res, r, 1);
        cfg.keyword           = PQgetvalue(res, r, 2);
        cfg.timeout_sec       = std::stoi(PQgetvalue(res, r, 3));
        cfg.notify_email      = PQgetvalue(res, r, 4);
        cfg.check_interval_min= std::stoi(PQgetvalue(res, r, 5));
        // PQgetvalue returns "t"/"f" for boolean in PostgreSQL
        cfg.is_active         = (PQgetvalue(res, r, 6)[0] == 't');
        configs.push_back(cfg);
    }

    PQclear(res);
    return configs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: Insert a monitoring_job row and return its generated ID.
//
// DBMS Concept: INSERT with RETURNING to get auto-generated PK.
// Returns 0 if DB is unavailable (task still pushed with id=0).
// ─────────────────────────────────────────────────────────────────────────────
int Scheduler::insert_monitoring_job(int website_id) {
    if (!pg_conn_ ||
        PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK) {
        return 0;
    }

    // Parameterized query — no string interpolation to prevent SQL injection
    std::string ws_id_str = std::to_string(website_id);
    const char* params[]  = { ws_id_str.c_str() };

    PGresult* res = PQexecParams(
        static_cast<PGconn*>(pg_conn_),
        "INSERT INTO monitoring_jobs (website_id, scheduled_at, status) "
        "VALUES ($1::int, NOW(), 'pending') "
        "RETURNING id",
        1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        std::cerr << "[Scheduler] insert_monitoring_job failed: "
                  << PQerrorMessage(static_cast<PGconn*>(pg_conn_)) << "\n";
        PQclear(res);
        return 0;
    }

    int job_id = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return job_id;
}

// ─────────────────────────────────────────────────────────────────────────────
// FixedIntervalStrategy
// Check if (now - last_checked) >= interval_min * 60 seconds.
// ─────────────────────────────────────────────────────────────────────────────
bool FixedIntervalStrategy::should_check(int /*website_id*/,
                                          long long last_checked,
                                          int       interval_min,
                                          long long now) const {
    return (now - last_checked) >=
           (static_cast<long long>(interval_min) * 60LL);
}

// ─────────────────────────────────────────────────────────────────────────────
// AdaptiveStrategy
// Doubles the effective check frequency when recent failure rate > 50%.
// Uses the injected failure_rate_fn_ callable for per-site failure rate lookup.
// ─────────────────────────────────────────────────────────────────────────────
AdaptiveStrategy::AdaptiveStrategy(FailureRateFn fn)
    : failure_rate_fn_(fn ? fn : [](int) { return 0.0; }) {}

bool AdaptiveStrategy::should_check(int       website_id,
                                     long long last_checked,
                                     int       interval_min,
                                     long long now) const {
    // Base interval in seconds
    long long base_interval = static_cast<long long>(interval_min) * 60LL;

    // Query the injected callable for recent failure rate [0.0, 1.0].
    // If > 50% failures, halve the interval (check twice as often).
    double failure_rate = failure_rate_fn_(website_id);

    long long effective_interval = (failure_rate > 0.5)
                                   ? base_interval / 2LL
                                   : base_interval;

    return (now - last_checked) >= effective_interval;
}

} // namespace webnotifier
