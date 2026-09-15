// ============================================================
// scheduler/include/scheduler.h
// Task Scheduler - Cron-like Job Dispatcher (Daemon)
// Owner: Rishita Ramola
//
// OS Concepts Demonstrated:
//   - Daemon thread      (std::thread)
//   - Mutex              (std::mutex protecting website list)
//   - Tick-based polling (sleep_for + TICK_INTERVAL_SEC)
//   - Exponential backoff retry on DB failure
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <memory>
#include "scheduling_strategy.h"
#include "../../queue/include/task_queue.h"

namespace webnotifier {

/**
 * @brief Configuration for one monitored website,
 *        loaded from the `websites` table in PostgreSQL.
 */
struct WebsiteConfig {
    int         website_id;
    std::string url;
    std::string keyword;
    int         timeout_sec;
    std::string notify_email;
    int         check_interval_min;   ///< How often to check (minutes)
    bool        is_active;
};

/**
 * @brief Cron-like Task Scheduler daemon.
 *
 * Loads active URLs from the PostgreSQL `websites` table,
 * uses a SchedulingStrategy to determine which are overdue,
 * and pushes Task objects into the shared TaskQueue for
 * Shivank's Worker Pool to consume.
 *
 * OS Concepts:
 *   - daemon thread via std::thread
 *   - mutex-protected website list
 *   - tick-based 60-second sleep loop
 *   - exponential backoff on DB connection loss
 */
class Scheduler {
public:
    /**
     * @param queue      Shared thread-safe TaskQueue (Shivank's contract).
     * @param strategy   Pluggable scheduling algorithm (default: FixedInterval).
     */
    explicit Scheduler(TaskQueue& queue,
                       std::shared_ptr<SchedulingStrategy> strategy = nullptr);
    ~Scheduler();

    // Non-copyable, non-movable (owns a live thread)
    Scheduler(const Scheduler&)            = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /** @brief Connect to DB and launch background daemon thread. */
    void start();

    /** @brief Signal daemon to stop and join the thread. */
    void stop();

    /**
     * @brief Hot-reload website configs from the database.
     * Safe to call from any thread — internally acquires mutex_.
     */
    void reload_websites();

    /**
     * @brief Manually add a website config (used by API layer / tests).
     * Thread-safe.
     */
    void add_website(const WebsiteConfig& config);

    /** @brief Returns true if the daemon thread is running. */
    bool is_running() const;

private:
    // ── Core daemon loop ────────────────────────────────────
    void run();

    /**
     * @brief Determine whether a website is due for a check.
     * Delegates to strategy_.
     */
    bool is_due(const WebsiteConfig& config, long long now_sec) const;

    // ── Database helpers ─────────────────────────────────────
    /**
     * @brief Load active website configs from PostgreSQL.
     * Implements exponential-backoff retry if connection is lost.
     * @return Vector of configs; empty if DB is unreachable after all retries.
     */
    std::vector<WebsiteConfig> load_from_db();

    /**
     * @brief Attempt to (re)connect to PostgreSQL with exponential backoff.
     * Waits 1s, 2s, 4s, 8s, 16s … up to MAX_BACKOFF_SEC between retries.
     * @return true if connection succeeded.
     */
    bool connect_with_retry();

    /**
     * @brief Insert a row into monitoring_jobs and return the new job id.
     * Returns 0 on failure (caller still pushes task with id=0).
     */
    int insert_monitoring_job(int website_id);

    // ── Data members ─────────────────────────────────────────
    TaskQueue&                              queue_;
    std::shared_ptr<SchedulingStrategy>     strategy_;
    std::vector<WebsiteConfig>              websites_;
    std::unordered_map<int, long long>      last_checked_;  ///< website_id → epoch-sec

    mutable std::mutex                      mutex_;
    std::thread                             daemon_thread_;
    std::atomic<bool>                       running_;

    // libpq connection — stored as void* so <libpq-fe.h> is not
    // exposed to consumers of this header.
    void*       pg_conn_{ nullptr };
    std::string db_conn_string_;   ///< Built from env vars at construction

    // ── Timing constants ─────────────────────────────────────
    static constexpr int TICK_INTERVAL_SEC = 60;   ///< Main loop cadence
    static constexpr int MAX_BACKOFF_SEC   = 64;   ///< Cap for retry backoff
    static constexpr int MAX_DB_RETRIES    = 6;    ///< Retries before giving up per cycle

    // ── Pacing: avoid flooding queue on first boot ───────────
    static constexpr int PUSH_PACE_MS     = 50;    ///< ms between consecutive pushes
};

} // namespace webnotifier
