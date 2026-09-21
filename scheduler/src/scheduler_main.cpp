// ============================================================
// scheduler/src/scheduler_main.cpp
// WebNotifier — Scheduler + Worker Pool Daemon
// ============================================================

#include "../include/scheduler.h"
#include "../../worker_pool/include/worker_pool.h"

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

        std::cout << "[Daemon] Using AdaptiveStrategy.\n";

        strategy =
            std::make_shared<webnotifier::AdaptiveStrategy>();

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