// ============================================================
// scheduler/src/scheduler_main.cpp
// Scheduler Daemon — Entry Point
// Owner: Rishita Ramola
//
// Compiled to the `scheduler_daemon` binary.
// Launched by scripts/start_scheduler.sh (nohup / background).
//
// OS Concepts:
//   - Signal handling: SIGINT/SIGTERM → graceful stop
//   - Environment variable consumption
//   - Daemon lifecycle management
// ============================================================
#include "../scheduler/include/scheduler.h"
#include <iostream>
#include <sstream>
#include <csignal>   // signal(), SIGINT, SIGTERM
#include <cstdlib>   // getenv
#include <thread>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// Global scheduler pointer for signal handler.
// (Acceptable for a signal handler — keep it simple.)
// ─────────────────────────────────────────────────────────────────────────────
static webnotifier::Scheduler* g_scheduler = nullptr;

static void signal_handler(int sig) {
    std::cout << "\n[Daemon] Received signal " << sig
              << " — stopping scheduler...\n";
    if (g_scheduler) g_scheduler->stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Build DB connection string from environment variables
// ─────────────────────────────────────────────────────────────────────────────
static std::string build_conn_string() {
    auto env = [](const char* name, const char* dflt = "") -> std::string {
        const char* v = std::getenv(name);
        return v ? v : dflt;
    };
    std::ostringstream oss;
    oss << "host="      << env("DB_HOST",     "localhost")
        << " port="     << env("DB_PORT",     "5432")
        << " dbname="   << env("DB_NAME",     "webnotifier")
        << " user="     << env("DB_USER",     "postgres")
        << " password=" << env("DB_PASSWORD", "")
        << " sslmode="  << env("DB_SSLMODE",  "prefer")
        << " connect_timeout=5";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "============================================\n"
              << " WebNotifier — Scheduler Daemon\n"
              << " Owner: Rishita Ramola\n"
              << "============================================\n";

    // Install signal handlers for graceful shutdown
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Read queue capacity from env (default 1000)
    const char* cap_env = std::getenv("QUEUE_CAPACITY");
    size_t queue_cap    = cap_env ? static_cast<size_t>(std::stoul(cap_env))
                                  : 1000;

    // Instantiate Shivank's TaskQueue (shared contract)
    webnotifier::TaskQueue task_queue(queue_cap);

    // Choose scheduling strategy from env
    // SCHEDULER_STRATEGY=adaptive  → AdaptiveStrategy
    // (anything else or unset)     → FixedIntervalStrategy
    std::shared_ptr<webnotifier::SchedulingStrategy> strategy;
    const char* strat_env = std::getenv("SCHEDULER_STRATEGY");
    if (strat_env && std::string(strat_env) == "adaptive") {
        std::cout << "[Daemon] Using AdaptiveStrategy.\n";
        strategy = std::make_shared<webnotifier::AdaptiveStrategy>();
    } else {
        std::cout << "[Daemon] Using FixedIntervalStrategy.\n";
        strategy = std::make_shared<webnotifier::FixedIntervalStrategy>();
    }

    // Construct and start the Scheduler
    webnotifier::Scheduler scheduler(task_queue, strategy);
    g_scheduler = &scheduler;

    scheduler.start();

    // Block main thread — daemon loop runs in scheduler's background thread.
    // Spin-wait cheaply on is_running(); signals will flip it via stop().
    while (scheduler.is_running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[Daemon] Exiting cleanly.\n";
    return 0;
}
