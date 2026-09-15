// ============================================================
// analytics/src/analytics_engine.cpp
// Analytics Engine — Full Implementation
// Owner: Rishita Ramola
//
// DBMS Concepts:
//   - Aggregation queries (COUNT, AVG, SUM, ROUND, GROUP BY)
//   - Rolling window date filtering (NOW() - INTERVAL 'N days')
//   - Parameterized queries (PQexecParams — prevents SQL injection)
//   - INSERT … RETURNING for atomic save + ID retrieval
//   - LEFT JOIN for outer aggregation (sites with zero checks)
//
// OS / Systems Concepts:
//   - std::filesystem for directory creation and path management
//   - popen() for sub-process invocation (wkhtmltopdf)
//   - RAII via destructors (PGresult always cleared, file streams closed)
//   - Environment variables for credentials (no hardcoding)
//   - libcurl for HTTP POST to Divyansh's internal email API
//
// Dependencies:
//   - Reads from: websites, monitoring_results, alerts (Divyansh's schema)
//   - Writes to:  weekly_reports (shared schema)
//   - Calls:      POST /api/internal/send-report (Divyansh's API)
// ============================================================
#include "../include/analytics_engine.h"
#include <libpq-fe.h>
#include <curl/curl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>   // getenv
#include <cstdio>    // popen, pclose

namespace webnotifier {

// ─────────────────────────────────────────────────────────────────────────────
// RAII wrapper for PGresult — ensures PQclear is always called.
// ─────────────────────────────────────────────────────────────────────────────
struct PGResultGuard {
    PGresult* res;
    explicit PGResultGuard(PGresult* r) : res(r) {}
    ~PGResultGuard() { if (res) PQclear(res); }
    // Non-copyable
    PGResultGuard(const PGResultGuard&)            = delete;
    PGResultGuard& operator=(const PGResultGuard&) = delete;
};

// ─────────────────────────────────────────────────────────────────────────────
// libcurl write callback — accumulates HTTP response body into a std::string.
// ─────────────────────────────────────────────────────────────────────────────
static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: safely convert a DB string to double; returns default_val if empty.
// Guards against divide-by-zero / stod exception on NULL columns.
// ─────────────────────────────────────────────────────────────────────────────
static double safe_double(const std::string& s, double default_val = 0.0) {
    if (s.empty()) return default_val;
    try { return std::stod(s); }
    catch (...) { return default_val; }
}

static int safe_int(const std::string& s, int default_val = 0) {
    if (s.empty()) return default_val;
    try { return std::stoi(s); }
    catch (...) { return default_val; }
}

static long long safe_ll(const std::string& s, long long default_val = 0) {
    if (s.empty()) return default_val;
    try { return std::stoll(s); }
    catch (...) { return default_val; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
AnalyticsEngine::AnalyticsEngine(const std::string& db_conn_string,
                                 const std::string& reports_dir)
    : db_conn_string_(db_conn_string),
      conn_(nullptr),
      reports_dir_(reports_dir) {

    // Connect to PostgreSQL
    PGconn* conn = PQconnectdb(db_conn_string_.c_str());
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "[Analytics] DB connection failed: "
                  << PQerrorMessage(conn) << "\n";
        PQfinish(conn);
        // conn_ remains nullptr — all methods check before querying
    } else {
        conn_ = conn;
        std::cout << "[Analytics] DB connected.\n";
    }

    // Create reports directory if it does not exist (OS file management)
    std::error_code ec;
    std::filesystem::create_directories(reports_dir_, ec);
    if (ec) {
        std::cerr << "[Analytics] WARNING: Cannot create reports dir '"
                  << reports_dir_ << "': " << ec.message() << "\n";
    }
}

AnalyticsEngine::~AnalyticsEngine() {
    // RAII: always release DB connection
    if (conn_) {
        PQfinish(static_cast<PGconn*>(conn_));
        conn_ = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1a — 7-day uptime + average response time for ONE website
//
// DBMS: COUNT(*), SUM(CASE…), AVG(response_time_ms)
//       Rolling window: WHERE checked_at >= NOW() - INTERVAL 'N days'
// ─────────────────────────────────────────────────────────────────────────────
UptimeStats AnalyticsEngine::get_uptime_stats(int website_id, int days) {
    UptimeStats stats{};
    stats.website_id = website_id;

    std::string ws_id_str  = std::to_string(website_id);
    std::string days_str   = std::to_string(days);

    // Query 1: aggregate uptime and TTFB
    auto rows = query_params(
        "SELECT "
        "  COUNT(*)                                                   AS total, "
        "  SUM(CASE WHEN status = 'UP'   THEN 1 ELSE 0 END)          AS up_count, "
        "  ROUND("
        "    100.0 * SUM(CASE WHEN status = 'UP' THEN 1 ELSE 0 END)"
        "           / NULLIF(COUNT(*), 0), 2"
        "  )                                                          AS uptime_pct, "
        "  COALESCE(AVG(response_time_ms), 0)                        AS avg_ms "
        "FROM monitoring_results "
        "WHERE website_id = $1::int "
        "  AND checked_at >= NOW() - ($2 || ' days')::interval",
        { ws_id_str, days_str });

    if (!rows.empty()) {
        stats.total_checks    = safe_int(rows[0]["total"]);
        stats.up_count        = safe_int(rows[0]["up_count"]);
        // Guard: if total_checks == 0, uptime_pct from NULLIF is NULL → 0.0
        stats.uptime_pct      = safe_double(rows[0]["uptime_pct"], 0.0);
        stats.avg_response_ms = safe_ll(rows[0]["avg_ms"]);
    }

    // Query 2: fetch URL and name for display
    auto meta = query_params(
        "SELECT url, name FROM websites WHERE id = $1::int",
        { ws_id_str });
    if (!meta.empty()) {
        stats.url  = meta[0]["url"];
        stats.name = meta[0]["name"];
    }

    // Query 3: downtime incidents count (Task 1b)
    stats.downtime_incidents = get_downtime_incidents(website_id, days);

    // Estimate downtime in minutes:
    // downtime_incidents × check_interval_min (from websites table).
    // This is an approximation; exact calculation requires consecutive-gap analysis.
    auto interval_rows = query_params(
        "SELECT check_interval_min FROM websites WHERE id = $1::int",
        { ws_id_str });
    if (!interval_rows.empty()) {
        int interval = safe_int(interval_rows[0]["check_interval_min"], 5);
        stats.downtime_minutes = static_cast<long long>(stats.downtime_incidents)
                                 * interval;
    }

    // Query 4: alert count for this website in the window
    auto alert_rows = query_params(
        "SELECT COUNT(*) AS cnt FROM alerts "
        "WHERE website_id = $1::int "
        "  AND sent_at >= NOW() - ($2 || ' days')::interval",
        { ws_id_str, days_str });
    if (!alert_rows.empty()) {
        stats.total_alerts = safe_int(alert_rows[0]["cnt"]);
    }

    return stats;
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1b — Downtime incident count for ONE website
//
// DBMS: COUNT(*) WHERE status = 'DOWN' AND date-window filter
// ─────────────────────────────────────────────────────────────────────────────
int AnalyticsEngine::get_downtime_incidents(int website_id, int days) {
    auto rows = query_params(
        "SELECT COUNT(*) AS cnt "
        "FROM monitoring_results "
        "WHERE website_id = $1::int "
        "  AND status      = 'DOWN' "
        "  AND checked_at >= NOW() - ($2 || ' days')::interval",
        { std::to_string(website_id), std::to_string(days) });

    if (rows.empty()) return 0;
    return safe_int(rows[0]["cnt"]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1c — Average response time (TTFB) for ONE website
//
// DBMS: AVG(response_time_ms) with rolling window
// ─────────────────────────────────────────────────────────────────────────────
double AnalyticsEngine::get_avg_response_time(int website_id, int days) {
    auto rows = query_params(
        "SELECT COALESCE(AVG(response_time_ms), 0) AS avg_ms "
        "FROM monitoring_results "
        "WHERE website_id = $1::int "
        "  AND status     != 'TIMEOUT' "        // exclude outliers
        "  AND checked_at >= NOW() - ($2 || ' days')::interval",
        { std::to_string(website_id), std::to_string(days) });

    if (rows.empty()) return 0.0;
    return safe_double(rows[0]["avg_ms"]);
}

// ─────────────────────────────────────────────────────────────────────────────
// ALL websites for a user — JOIN + GROUP BY aggregation
//
// DBMS: LEFT JOIN ensures websites with ZERO checks still appear (uptime=0).
//       GROUP BY w.id, w.url, w.name
// ─────────────────────────────────────────────────────────────────────────────
std::vector<UptimeStats> AnalyticsEngine::get_all_stats(int user_id, int days) {
    // Main aggregation query
    auto rows = query_params(
        "SELECT "
        "  w.id                                                             AS id, "
        "  w.url                                                            AS url, "
        "  w.name                                                           AS name, "
        "  COUNT(mr.id)                                                     AS total, "
        "  SUM(CASE WHEN mr.status = 'UP'   THEN 1 ELSE 0 END)             AS up_count, "
        "  SUM(CASE WHEN mr.status = 'DOWN' THEN 1 ELSE 0 END)             AS down_count, "
        "  ROUND("
        "    100.0 * SUM(CASE WHEN mr.status = 'UP' THEN 1 ELSE 0 END)"
        "           / NULLIF(COUNT(mr.id), 0), 2"
        "  )                                                                AS uptime_pct, "
        "  COALESCE(AVG(mr.response_time_ms), 0)                           AS avg_ms, "
        "  w.check_interval_min                                             AS interval_min "
        "FROM websites w "
        "LEFT JOIN monitoring_results mr "
        "       ON mr.website_id = w.id "
        "      AND mr.checked_at >= NOW() - ($2 || ' days')::interval "
        "WHERE w.user_id = $1::int "
        "GROUP BY w.id, w.url, w.name, w.check_interval_min "
        "ORDER BY w.id",
        { std::to_string(user_id), std::to_string(days) });

    // Alert count per website — separate query to keep main query clean
    auto alert_rows = query_params(
        "SELECT a.website_id, COUNT(a.id) AS cnt "
        "FROM alerts a "
        "JOIN websites w ON a.website_id = w.id "
        "WHERE w.user_id = $1::int "
        "  AND a.sent_at >= NOW() - ($2 || ' days')::interval "
        "GROUP BY a.website_id",
        { std::to_string(user_id), std::to_string(days) });

    // Build a lookup map: website_id → alert_count
    std::map<int, int> alert_map;
    for (const auto& ar : alert_rows)
        alert_map[safe_int(ar.at("website_id"))] = safe_int(ar.at("cnt"));

    std::vector<UptimeStats> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
        UptimeStats s;
        s.website_id         = safe_int(row.at("id"));
        s.url                = row.at("url");
        s.name               = row.at("name");
        s.total_checks       = safe_int(row.at("total"));
        s.up_count           = safe_int(row.at("up_count"));
        s.downtime_incidents = safe_int(row.at("down_count"));
        s.uptime_pct         = safe_double(row.at("uptime_pct"), 0.0);
        s.avg_response_ms    = safe_ll(row.at("avg_ms"));
        int interval_min     = safe_int(row.at("interval_min"), 5);
        s.downtime_minutes   = static_cast<long long>(s.downtime_incidents)
                               * interval_min;
        s.total_alerts       = alert_map.count(s.website_id)
                               ? alert_map[s.website_id] : 0;
        result.push_back(std::move(s));
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate weekly report — orchestrates all sub-tasks
// ─────────────────────────────────────────────────────────────────────────────
WeeklyReport AnalyticsEngine::generate_weekly_report(int user_id,
                                                      const std::string& week_start,
                                                      const std::string& week_end) {
    WeeklyReport report;
    report.user_id      = user_id;
    report.week_start   = week_start;
    report.week_end     = week_end;
    report.generated_at = current_timestamp();

    std::cout << "[Analytics] Generating weekly report for user_id="
              << user_id << " (" << week_start << " → " << week_end << ")\n";

    // Aggregate stats for all websites in the 7-day window
    report.website_stats = get_all_stats(user_id, 7);

    // Edge-case: no data this week — return polite empty report
    if (report.website_stats.empty()) {
        std::cout << "[Analytics] No telemetry recorded this week for user_id="
                  << user_id << ". Generating empty report.\n";
        report.overall_uptime_pct  = 0.0;
        report.total_downtime_min  = 0;
        report.total_alerts        = 0;
    } else {
        double  total_uptime    = 0.0;
        long long total_down    = 0;
        int       total_alerts  = 0;
        for (const auto& s : report.website_stats) {
            total_uptime  += s.uptime_pct;
            total_down    += s.downtime_minutes;
            total_alerts  += s.total_alerts;
        }
        report.overall_uptime_pct = total_uptime
                                    / static_cast<double>(report.website_stats.size());
        report.total_downtime_min = total_down;
        report.total_alerts       = total_alerts;
    }

    // Export CSV
    report.csv_path = export_to_csv(report);

    // Export PDF (if wkhtmltopdf is available)
    report.report_path = export_to_pdf(report);
    if (report.report_path.empty()) {
        // Fall back to CSV path so dispatch still works
        report.report_path = report.csv_path;
    }

    // Persist report metadata in weekly_reports table
    save_report_to_db(report);

    return report;
}

// ─────────────────────────────────────────────────────────────────────────────
// Export to CSV — OS: file I/O with ofstream (RAII, auto-closed on scope exit)
// ─────────────────────────────────────────────────────────────────────────────
std::string AnalyticsEngine::export_to_csv(const WeeklyReport& report) {
    std::string filename = reports_dir_
                         + "report_user_" + std::to_string(report.user_id)
                         + "_" + report.week_start + ".csv";

    std::ofstream f(filename);
    if (!f.is_open()) {
        std::cerr << "[Analytics] ERROR: Cannot open '" << filename
                  << "' for writing.\n";
        return "";
    }

    // Header row
    f << "website_id,url,name,total_checks,up_count,downtime_incidents,"
         "uptime_pct,avg_response_ms,downtime_minutes,total_alerts\n";

    if (report.website_stats.empty()) {
        f << ",,No telemetry recorded this week.,,,,,,\n";
    } else {
        for (const auto& s : report.website_stats) {
            // Escape URL commas/quotes minimally
            std::string safe_url  = s.url;
            std::string safe_name = s.name;
            if (safe_url.find(',')  != std::string::npos) safe_url  = '"' + safe_url  + '"';
            if (safe_name.find(',') != std::string::npos) safe_name = '"' + safe_name + '"';

            f << s.website_id       << ","
              << safe_url           << ","
              << safe_name          << ","
              << s.total_checks     << ","
              << s.up_count         << ","
              << s.downtime_incidents << ","
              << std::fixed << std::setprecision(2) << s.uptime_pct << ","
              << s.avg_response_ms  << ","
              << s.downtime_minutes << ","
              << s.total_alerts     << "\n";
        }
        // Summary row
        f << "OVERALL,,,,,,"
          << std::fixed << std::setprecision(2) << report.overall_uptime_pct
          << ",," << report.total_downtime_min << "," << report.total_alerts << "\n";
    }
    // ofstream destructor closes file (RAII)
    std::cout << "[Analytics] CSV exported: " << filename << "\n";
    return filename;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build HTML for the report (used as input to wkhtmltopdf)
// ─────────────────────────────────────────────────────────────────────────────
std::string AnalyticsEngine::build_html_report(const WeeklyReport& report) const {
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html>\n<head>\n"
      << "<meta charset='UTF-8'>\n"
      << "<style>\n"
      << "  body { font-family: Arial, sans-serif; margin: 40px; color: #333; }\n"
      << "  h1   { color: #1a73e8; }\n"
      << "  h2   { color: #555; border-bottom: 1px solid #ccc; padding-bottom:4px; }\n"
      << "  table{ border-collapse: collapse; width:100%; margin-top:16px; }\n"
      << "  th   { background:#1a73e8; color:#fff; padding:8px 12px; text-align:left; }\n"
      << "  td   { padding:7px 12px; border-bottom: 1px solid #eee; }\n"
      << "  tr:hover td { background:#f5f5f5; }\n"
      << "  .up   { color: #0a8a0a; font-weight:bold; }\n"
      << "  .warn { color: #cc6600; font-weight:bold; }\n"
      << "  .down { color: #cc0000; font-weight:bold; }\n"
      << "  .summary { background:#f0f4ff; padding:16px; border-radius:8px;"
         " margin-bottom:24px; }\n"
      << "</style>\n</head>\n<body>\n";

    h << "<h1>WebNotifier — Weekly Uptime Report</h1>\n";
    h << "<p>Generated: " << report.generated_at << "</p>\n";
    h << "<p>Period: <strong>" << report.week_start << "</strong> to "
      << "<strong>" << report.week_end << "</strong></p>\n";
    h << "<p>User ID: " << report.user_id << "</p>\n";

    // Summary box
    h << "<div class='summary'>\n";
    h << "<h2>Executive Summary</h2>\n";
    h << "<p>Overall Uptime: <strong>"
      << std::fixed << std::setprecision(2) << report.overall_uptime_pct
      << "%</strong></p>\n";
    h << "<p>Total Downtime: <strong>" << report.total_downtime_min
      << " minutes</strong></p>\n";
    h << "<p>Total Alerts Fired: <strong>" << report.total_alerts
      << "</strong></p>\n";
    h << "</div>\n";

    if (report.website_stats.empty()) {
        h << "<p><em>No telemetry recorded this week.</em></p>\n";
    } else {
        h << "<h2>Per-Website Statistics (Last 7 Days)</h2>\n";
        h << "<table>\n<tr>"
          << "<th>Website</th><th>URL</th><th>Checks</th>"
          << "<th>Uptime %</th><th>Avg Response (ms)</th>"
          << "<th>Downtime Incidents</th><th>Downtime (min)</th>"
          << "<th>Alerts</th></tr>\n";

        for (const auto& s : report.website_stats) {
            std::string cls = (s.uptime_pct >= 99.0) ? "up"
                            : (s.uptime_pct >= 95.0) ? "warn" : "down";
            h << "<tr>"
              << "<td>" << s.name       << "</td>"
              << "<td>" << s.url        << "</td>"
              << "<td>" << s.total_checks << "</td>"
              << "<td class='" << cls << "'>"
              << std::fixed << std::setprecision(2) << s.uptime_pct << "%</td>"
              << "<td>" << s.avg_response_ms   << "</td>"
              << "<td>" << s.downtime_incidents << "</td>"
              << "<td>" << s.downtime_minutes   << "</td>"
              << "<td>" << s.total_alerts       << "</td>"
              << "</tr>\n";
        }
        h << "</table>\n";
    }

    h << "</body>\n</html>\n";
    return h.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Export to PDF — OS: popen() sub-process, file permissions, temp file I/O
//
// Strategy:
//   1. Write HTML to a temp file in reports_dir_.
//   2. Invoke wkhtmltopdf via popen(); capture exit code.
//   3. Delete temp HTML (clean up).
//   4. Return PDF path, or "" if wkhtmltopdf is not available.
//
// The wkhtmltopdf binary path is read from env var WKHTMLTOPDF_PATH
// (defaults to "wkhtmltopdf" on PATH).
// ─────────────────────────────────────────────────────────────────────────────
std::string AnalyticsEngine::export_to_pdf(const WeeklyReport& report) {
    // Determine wkhtmltopdf binary
    const char* wk_env   = std::getenv("WKHTMLTOPDF_PATH");
    std::string wk_bin   = wk_env ? wk_env : "wkhtmltopdf";

    std::string base     = "report_user_" + std::to_string(report.user_id)
                         + "_" + report.week_start;
    std::string html_path = reports_dir_ + base + ".html";
    std::string pdf_path  = reports_dir_ + base + ".pdf";

    // Step 1 — Write HTML to disk
    {
        std::ofstream hf(html_path);
        if (!hf.is_open()) {
            std::cerr << "[Analytics] Cannot write HTML to '" << html_path << "'\n";
            return "";
        }
        hf << build_html_report(report);
        // hf closed here by RAII
    }

    // Step 2 — Invoke wkhtmltopdf
    // Build command safely — avoid shell injection by using fixed paths
    std::string cmd = wk_bin + " --quiet "
                    + "\"" + html_path + "\" "
                    + "\"" + pdf_path  + "\" 2>&1";

    std::cout << "[Analytics] Running: " << cmd << "\n";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[Analytics] popen() failed — wkhtmltopdf not available.\n";
        std::filesystem::remove(html_path);   // clean up temp HTML
        return "";
    }

    // Drain any output from the command (error messages)
    char buf[256];
    std::string cmd_output;
    while (fgets(buf, sizeof(buf), pipe)) {
        cmd_output += buf;
    }
    int exit_code = pclose(pipe);

    // Step 3 — Clean up temp HTML
    std::error_code ec;
    std::filesystem::remove(html_path, ec);

    if (exit_code != 0) {
        std::cerr << "[Analytics] wkhtmltopdf failed (exit=" << exit_code
                  << "): " << cmd_output << "\n";
        return "";
    }

    std::cout << "[Analytics] PDF exported: " << pdf_path << "\n";
    return pdf_path;
}

// ─────────────────────────────────────────────────────────────────────────────
// Generate summary JSON for dashboard API (Divyansh's endpoint reads this)
// ─────────────────────────────────────────────────────────────────────────────
std::string AnalyticsEngine::generate_summary_json(int user_id) {
    auto stats = get_all_stats(user_id, 7);
    std::ostringstream json;
    json << "{\"user_id\":" << user_id << ",\"websites\":[";
    for (size_t i = 0; i < stats.size(); ++i) {
        const auto& s = stats[i];
        if (i > 0) json << ",";
        json << "{"
             << "\"website_id\":"       << s.website_id      << ","
             << "\"url\":\""            << s.url             << "\","
             << "\"name\":\""           << s.name            << "\","
             << "\"total_checks\":"     << s.total_checks    << ","
             << "\"uptime_pct\":"
             << std::fixed << std::setprecision(2) << s.uptime_pct << ","
             << "\"avg_response_ms\":"  << s.avg_response_ms << ","
             << "\"downtime_incidents\":"<< s.downtime_incidents << ","
             << "\"downtime_minutes\":"  << s.downtime_minutes   << ","
             << "\"total_alerts\":"      << s.total_alerts
             << "}";
    }
    json << "]}";
    return json.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Save report metadata to weekly_reports table
//
// DBMS: Parameterized INSERT with RETURNING id
// ─────────────────────────────────────────────────────────────────────────────
bool AnalyticsEngine::save_report_to_db(const WeeklyReport& report) {
    if (!conn_) {
        std::cerr << "[Analytics] save_report_to_db: no DB connection.\n";
        return false;
    }

    // Aggregate totals
    int total_checks = 0;
    long long avg_ms = 0;
    for (const auto& s : report.website_stats) {
        total_checks += s.total_checks;
        avg_ms       += s.avg_response_ms;
    }
    if (!report.website_stats.empty())
        avg_ms /= static_cast<long long>(report.website_stats.size());

    std::string uid         = std::to_string(report.user_id);
    std::string wstart      = report.week_start;
    std::string wend        = report.week_end;
    std::string total_chk_s = std::to_string(total_checks);
    std::string uptime_s    = std::to_string(report.overall_uptime_pct);
    std::string downtime_s  = std::to_string(report.total_downtime_min);
    std::string avg_ms_s    = std::to_string(avg_ms);
    std::string alerts_s    = std::to_string(report.total_alerts);
    std::string path_s      = report.report_path;

    bool ok = exec_params(
        "INSERT INTO weekly_reports "
        "  (user_id, week_start, week_end, total_checks, uptime_pct, "
        "   downtime_min, avg_response_ms, total_alerts, report_path) "
        "VALUES "
        "  ($1::int, $2::date, $3::date, $4::int, $5::numeric, "
        "   $6::int, $7::bigint, $8::int, $9) "
        "ON CONFLICT DO NOTHING",
        { uid, wstart, wend, total_chk_s, uptime_s,
          downtime_s, avg_ms_s, alerts_s, path_s });

    if (ok)
        std::cout << "[Analytics] Report saved to DB for user_id="
                  << report.user_id << "\n";
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// Dispatch report to Divyansh's internal email API
//
// OS: libcurl HTTP POST
// Security: INTERNAL_API_HOST and INTERNAL_API_KEY from env vars.
// Contract: POST /api/internal/send-report
//   Body (JSON): { "user_id": N, "report_path": "...", "week_start": "..." }
//   Header:      X-Internal-Key: <INTERNAL_API_KEY>
// ─────────────────────────────────────────────────────────────────────────────
bool AnalyticsEngine::dispatch_report_email(const WeeklyReport& report) {
    const char* api_host = std::getenv("INTERNAL_API_HOST");
    const char* api_key  = std::getenv("INTERNAL_API_KEY");

    if (!api_host || !api_key) {
        std::cerr << "[Analytics] INTERNAL_API_HOST / INTERNAL_API_KEY not set. "
                     "Skipping email dispatch.\n";
        return false;
    }

    std::string url = std::string(api_host) + "/api/internal/send-report";

    // Build JSON payload
    std::ostringstream payload;
    payload << "{"
            << "\"user_id\":"     << report.user_id         << ","
            << "\"report_path\":\"" << report.report_path   << "\","
            << "\"week_start\":\"" << report.week_start     << "\","
            << "\"week_end\":\""   << report.week_end       << "\""
            << "}";
    std::string body = payload.str();

    // Initialise libcurl (OS: underlying TCP socket, TLS handshake)
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[Analytics] curl_easy_init() failed.\n";
        return false;
    }

    std::string response_body;
    long http_code = 0;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = std::string("X-Internal-Key: ") + api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,     body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,  static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[Analytics] Email dispatch curl error: "
                  << curl_easy_strerror(res) << "\n";
        return false;
    }

    bool success = (http_code >= 200 && http_code < 300);
    if (success)
        std::cout << "[Analytics] Email dispatch successful (HTTP "
                  << http_code << ").\n";
    else
        std::cerr << "[Analytics] Email dispatch failed: HTTP "
                  << http_code << " body=" << response_body << "\n";
    return success;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: execute a plain SELECT query (no parameters)
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::map<std::string,std::string>>
AnalyticsEngine::query(const std::string& sql) {
    return query_params(sql, {});
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: execute a parameterized SELECT
// DBMS / Security: PQexecParams prevents SQL injection.
// All NULL values are returned as empty string (callers use safe_*() helpers).
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::map<std::string,std::string>>
AnalyticsEngine::query_params(const std::string& sql,
                               const std::vector<std::string>& params) {
    std::vector<std::map<std::string,std::string>> rows;
    if (!conn_) return rows;

    // Build C-string array for PQexecParams
    std::vector<const char*> c_params;
    c_params.reserve(params.size());
    for (const auto& p : params)
        c_params.push_back(p.c_str());

    PGresult* raw = PQexecParams(
        static_cast<PGconn*>(conn_),
        sql.c_str(),
        static_cast<int>(c_params.size()),
        nullptr,              // let server infer types
        c_params.empty() ? nullptr : c_params.data(),
        nullptr,              // param lengths (text format)
        nullptr,              // param formats (text)
        0);                   // result format: text

    PGResultGuard guard(raw);  // RAII: PQclear on scope exit

    if (PQresultStatus(raw) != PGRES_TUPLES_OK) {
        std::cerr << "[Analytics] Query error: "
                  << PQerrorMessage(static_cast<PGconn*>(conn_)) << "\n"
                  << "  SQL: " << sql.substr(0, 120) << "...\n";
        return rows;
    }

    int nrows = PQntuples(raw);
    int ncols = PQnfields(raw);
    rows.reserve(static_cast<size_t>(nrows));

    for (int r = 0; r < nrows; ++r) {
        std::map<std::string,std::string> row;
        for (int c = 0; c < ncols; ++c) {
            // PQgetisnull returns 1 if the value is SQL NULL
            row[PQfname(raw, c)] = PQgetisnull(raw, r, c)
                                   ? "" : PQgetvalue(raw, r, c);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: execute a non-SELECT statement (INSERT/UPDATE)
// ─────────────────────────────────────────────────────────────────────────────
bool AnalyticsEngine::exec_params(const std::string& sql,
                                   const std::vector<std::string>& params) {
    if (!conn_) return false;

    std::vector<const char*> c_params;
    c_params.reserve(params.size());
    for (const auto& p : params) c_params.push_back(p.c_str());

    PGresult* raw = PQexecParams(
        static_cast<PGconn*>(conn_),
        sql.c_str(),
        static_cast<int>(c_params.size()),
        nullptr,
        c_params.empty() ? nullptr : c_params.data(),
        nullptr, nullptr, 0);

    PGResultGuard guard(raw);

    ExecStatusType status = PQresultStatus(raw);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::cerr << "[Analytics] exec_params error: "
                  << PQerrorMessage(static_cast<PGconn*>(conn_)) << "\n";
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: current UTC timestamp as ISO8601
// ─────────────────────────────────────────────────────────────────────────────
std::string AnalyticsEngine::current_timestamp() const {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace webnotifier
