// ============================================================
// system_monitor/src/system_monitor_main.cpp
// OS System Resource Monitor — Entry Point
// Owner: Simran Negi
//
// Compiled to: build/system_monitor_daemon
//
// OS Concepts Demonstrated:
//   - Signal handling : SIGINT / SIGTERM → graceful stop
//   - Environment variables : credentials from env, never hardcoded
//   - Daemon lifecycle : nohup + PID file (via start_monitor.sh)
// ============================================================
#include "../include/system_monitor.h"
#include <iostream>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>

// ─────────────────────────────────────────────────────────────────────────────
// Global pointer for signal handler
// ─────────────────────────────────────────────────────────────────────────────
static webnotifier::SystemMonitor* g_monitor = nullptr;

static void signal_handler(int sig) {
    std::cout << "\n[SysMonitor] Received signal " << sig
              << " — stopping...\n";
    if (g_monitor) g_monitor->stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Build PostgreSQL connection string from environment variables.
// Credentials are NEVER hardcoded.
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
              << " WebNotifier — System Resource Monitor\n"
              << " Owner: Simran Negi\n"
              << "============================================\n";

    // Install signal handlers for graceful shutdown
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Read config from environment variables
    const char* interval_env = std::getenv("MONITOR_INTERVAL_SEC");
    int interval_sec = interval_env ? std::stoi(interval_env) : 30;

    const char* process_env = std::getenv("SCHEDULER_PROCESS_NAME");
    std::string process_name = process_env ? process_env : "scheduler_daemon";

    std::cout << "[SysMonitor] interval=" << interval_sec << "s"
              << "  watching='" << process_name << "'\n";

    // Construct and run the monitor (blocks until stop() is called)
    webnotifier::SystemMonitor monitor(
        build_conn_string(), interval_sec, process_name);
    g_monitor = &monitor;

    monitor.run();   // blocks here

    std::cout << "[SysMonitor] Exiting cleanly.\n";
    return 0;
}
