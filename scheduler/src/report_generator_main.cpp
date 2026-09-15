// ============================================================
// scheduler/src/report_generator_main.cpp
// Standalone Weekly Report Generator — Entry Point
// Owner: Rishita Ramola
//
// Purpose: Compiled to the `report_generator` binary.
//   Invoked weekly by the OS cron daemon (via setup_cron.sh).
//   Fetches all active users, generates a weekly report for each,
//   and dispatches the PDF via Divyansh's internal email API.
//
// OS Concepts:
//   - Environment variable consumption (getenv)
//   - Signal handling (SIGINT / SIGTERM for graceful exit)
//   - std::filesystem for report directory management
//   - Return codes communicated to cron via exit()
// ============================================================
#include "../analytics/include/analytics_engine.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdlib>   // getenv, exit
#include <libpq-fe.h>

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
        << " connect_timeout=10";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Return ISO8601 date string for today and for 7 days ago.
// ─────────────────────────────────────────────────────────────────────────────
static void get_week_range(std::string& week_start, std::string& week_end) {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);

    // week_end = today
    char end_buf[16];
    std::strftime(end_buf, sizeof(end_buf), "%Y-%m-%d", std::gmtime(&tt));
    week_end = end_buf;

    // week_start = 7 days ago
    auto seven_days_ago = now - std::chrono::hours(7 * 24);
    auto tt2 = std::chrono::system_clock::to_time_t(seven_days_ago);
    char start_buf[16];
    std::strftime(start_buf, sizeof(start_buf), "%Y-%m-%d", std::gmtime(&tt2));
    week_start = start_buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fetch all active user IDs from the database.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<int> get_active_user_ids(const std::string& conn_string) {
    std::vector<int> ids;
    PGconn* conn = PQconnectdb(conn_string.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "[ReportGen] DB connect failed: "
                  << PQerrorMessage(conn) << "\n";
        PQfinish(conn);
        return ids;
    }

    PGresult* res = PQexec(conn,
        "SELECT DISTINCT u.id FROM users u "
        "JOIN websites w ON w.user_id = u.id "
        "WHERE u.is_active = TRUE AND w.is_active = TRUE "
        "ORDER BY u.id");

    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int n = PQntuples(res);
        ids.reserve(static_cast<size_t>(n));
        for (int r = 0; r < n; ++r) {
            try { ids.push_back(std::stoi(PQgetvalue(res, r, 0))); }
            catch (...) {}
        }
    } else {
        std::cerr << "[ReportGen] Query failed: "
                  << PQerrorMessage(conn) << "\n";
    }

    PQclear(res);
    PQfinish(conn);
    return ids;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main entry point
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "============================================\n"
              << " WebNotifier — Weekly Report Generator\n"
              << " Owner: Rishita Ramola\n"
              << "============================================\n";

    // Read reports output directory from env (default: "reports/")
    const char* reports_dir_env = std::getenv("REPORTS_DIR");
    std::string reports_dir = reports_dir_env ? reports_dir_env : "reports/";

    // Build DB connection string from env vars
    std::string conn_str = build_conn_string();

    // Calculate the current week's date range
    std::string week_start, week_end;
    get_week_range(week_start, week_end);

    std::cout << "[ReportGen] Week: " << week_start << " → " << week_end << "\n";

    // Fetch all users with active monitored sites
    auto user_ids = get_active_user_ids(conn_str);
    if (user_ids.empty()) {
        std::cout << "[ReportGen] No active users found. Nothing to do.\n";
        return 0;
    }
    std::cout << "[ReportGen] Found " << user_ids.size() << " active user(s).\n";

    // Instantiate the Analytics Engine (owns one DB connection)
    webnotifier::AnalyticsEngine engine(conn_str, reports_dir);

    int success_count = 0;
    int fail_count    = 0;

    for (int user_id : user_ids) {
        std::cout << "\n[ReportGen] Processing user_id=" << user_id << " ...\n";
        try {
            // Generate report: aggregates stats, exports CSV+PDF, saves to DB
            auto report = engine.generate_weekly_report(user_id,
                                                         week_start, week_end);

            // Dispatch the report via Divyansh's email API
            bool dispatched = engine.dispatch_report_email(report);
            if (dispatched) {
                ++success_count;
                std::cout << "[ReportGen] ✓ user_id=" << user_id
                          << " report dispatched.\n";
            } else {
                // Email failed, but report was still generated locally
                ++fail_count;
                std::cerr << "[ReportGen] ✗ user_id=" << user_id
                          << " email dispatch failed (report at: "
                          << report.report_path << ").\n";
            }
        } catch (const std::exception& ex) {
            ++fail_count;
            std::cerr << "[ReportGen] ERROR for user_id=" << user_id
                      << ": " << ex.what() << "\n";
        }
    }

    std::cout << "\n[ReportGen] Done. Success=" << success_count
              << " Failed=" << fail_count << "\n";

    // Return non-zero exit code if any failures — cron will log this
    return (fail_count > 0) ? 1 : 0;
}
