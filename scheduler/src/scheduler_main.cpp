// ============================================================
// scheduler/src/scheduler_main.cpp
// WebNotifier — Scheduler + Worker Pool Daemon
// ============================================================

#include "../include/scheduler.h"
#include "../../worker_pool/include/worker_pool.h"

#include <libpq-fe.h>
#include <iostream>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <string>

// ─────────────────────────────────────────────────────────────
// Global scheduler pointer for signal handler
// ─────────────────────────────────────────────────────────────
static webnotifier::Scheduler* g_scheduler = nullptr;

static void signal_handler(int sig)
{
    std::cout << "\n[Daemon] Received signal " << sig
              << " — stopping WebNotifier...\n";

    if (g_scheduler) {
        g_scheduler->stop();
    }
}

// ─────────────────────────────────────────────────────────────
// Build PostgreSQL connection string from environment variables
// ─────────────────────────────────────────────────────────────
static std::string build_conn_string()
{
    auto env = [](const char* name, const char* dflt = "") -> std::string {
        const char* v = std::getenv(name);
        return v ? v : dflt;
    };

    std::ostringstream oss;

    oss << "host="      << env("DB_HOST", "localhost")
        << " port="     << env("DB_PORT", "5432")
        << " dbname="   << env("DB_NAME", "webnotifier")
        << " user="     << env("DB_USER", "postgres")
        << " password=" << env("DB_PASSWORD", "")
        << " sslmode="  << env("DB_SSLMODE", "prefer")
        << " connect_timeout=5";

    return oss.str();
}

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────
int main()
{
    // Flush stdout on every insertion so logs are visible in real time even
    // when piped to a file (default C++ block-buffering otherwise hides them).
    std::cout << std::unitbuf;

    std::cout
        << "============================================\n"
        << " WebNotifier — Monitoring Daemon\n"
        << " Scheduler + Worker Pool\n"
        << "============================================\n";

    // ─────────────────────────────────────────────────────────
    // Install signal handlers
    // ─────────────────────────────────────────────────────────
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ─────────────────────────────────────────────────────────
    // Queue configuration
    // ─────────────────────────────────────────────────────────
    const char* cap_env = std::getenv("QUEUE_CAPACITY");

    std::size_t queue_capacity =
        cap_env
            ? static_cast<std::size_t>(std::stoul(cap_env))
            : 1000;

    // One shared queue for Scheduler + WorkerPool.
    webnotifier::TaskQueue task_queue(queue_capacity);

    // ─────────────────────────────────────────────────────────
    // Worker configuration
    // ─────────────────────────────────────────────────────────
    const char* worker_env = std::getenv("WORKER_COUNT");

    std::size_t worker_count =
        worker_env
            ? static_cast<std::size_t>(std::stoul(worker_env))
            : 4;

    if (worker_count == 0) {
        std::cerr << "[Daemon] ERROR: WORKER_COUNT must be greater than 0.\n";
        return 1;
    }

    std::cout << "[Daemon] Queue capacity : "
              << queue_capacity << '\n';

    std::cout << "[Daemon] Worker count   : "
              << worker_count << '\n';

    // ─────────────────────────────────────────────────────────
    // Database Writer
    // ─────────────────────────────────────────────────────────
    const std::string db_connection = build_conn_string();

    webnotifier::DatabaseWriter database_writer(db_connection);

    if (!database_writer.connect()) {
        std::cerr << "[Daemon] ERROR: Worker Pool database connection failed.\n";
        return 1;
    }

    // ─────────────────────────────────────────────────────────
    // Scheduling strategy
    // ─────────────────────────────────────────────────────────
    std::shared_ptr<webnotifier::SchedulingStrategy> strategy;

    const char* strat_env = std::getenv("SCHEDULER_STRATEGY");

    if (strat_env &&
        std::string(strat_env) == "adaptive") {

        std::cout << "[Daemon] Using AdaptiveStrategy (real failure-rate feed).\n";

        // Dedicated read-only connection for failure-rate lookups. Owned by the
        // lambda's enclosing scope (static) so it outlives the strategy object.
        static PGconn* rate_conn = PQconnectdb(db_connection.c_str());
        if (PQstatus(rate_conn) != CONNECTION_OK) {
            std::cerr << "[Daemon] WARNING: adaptive rate connection failed: "
                      << PQerrorMessage(rate_conn)
                      << " — failure rate will read as 0.\n";
        }

        // Fraction of non-UP results in the last hour, per website [0.0, 1.0].
        auto failure_rate_fn = [](int website_id) -> double {
            if (!rate_conn || PQstatus(rate_conn) != CONNECTION_OK) return 0.0;
            std::string id = std::to_string(website_id);
            const char* params[] = { id.c_str() };
            PGresult* r = PQexecParams(
                rate_conn,
                "SELECT COALESCE(AVG(CASE WHEN status <> 'UP' THEN 1.0 ELSE 0.0 END), 0.0) "
                "FROM monitoring_results "
                "WHERE website_id = $1::int "
                "  AND checked_at >= NOW() - INTERVAL '1 hour'",
                1, nullptr, params, nullptr, nullptr, 0);
            double rate = 0.0;
            if (PQresultStatus(r) == PGRES_TUPLES_OK && PQntuples(r) > 0) {
                rate = std::atof(PQgetvalue(r, 0, 0));
            }
            PQclear(r);
            return rate;
        };

        strategy =
            std::make_shared<webnotifier::AdaptiveStrategy>(failure_rate_fn);

    } else {

        std::cout << "[Daemon] Using FixedIntervalStrategy.\n";

        strategy =
            std::make_shared<webnotifier::FixedIntervalStrategy>();
    }

    // ─────────────────────────────────────────────────────────
    // Create Worker Pool
    //
    // IMPORTANT:
    // The Worker Pool receives the SAME TaskQueue instance
    // used by the Scheduler.
    // ─────────────────────────────────────────────────────────
    webnotifier::WorkerPool worker_pool(
        task_queue,
        database_writer,
        worker_count
    );

    // ─────────────────────────────────────────────────────────
    // Create Scheduler
    //
    // Scheduler receives the SAME TaskQueue instance.
    // ─────────────────────────────────────────────────────────
    webnotifier::Scheduler scheduler(
        task_queue,
        strategy
    );

    g_scheduler = &scheduler;

    // ─────────────────────────────────────────────────────────
    // Start both components
    // ─────────────────────────────────────────────────────────
    std::cout << "[Daemon] Starting Worker Pool...\n";
    worker_pool.start();

    std::cout << "[Daemon] Starting Scheduler...\n";
    scheduler.start();

    // ─────────────────────────────────────────────────────────
    // Keep main thread alive while Scheduler is running.
    // ─────────────────────────────────────────────────────────
    while (scheduler.is_running()) {
        std::this_thread::sleep_for(
            std::chrono::seconds(1)
        );
    }

    // ─────────────────────────────────────────────────────────
    // Graceful shutdown
    // ─────────────────────────────────────────────────────────
    std::cout << "[Daemon] Stopping Worker Pool...\n";
    worker_pool.stop();

    database_writer.disconnect();

    g_scheduler = nullptr;

    std::cout
        << "[Daemon] WebNotifier stopped cleanly.\n";

    return 0;
}