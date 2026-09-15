// ============================================================
// analytics/include/analytics_engine.h
// Analytics Engine — Data Access Object (DAO)
// Owner: Rishita Ramola
//
// DBMS Concepts Demonstrated:
//   - Aggregation     (COUNT, AVG, SUM, ROUND, GROUP BY)
//   - Date windowing  (NOW() - INTERVAL 'N days')
//   - Parameterized queries (prevent SQL injection)
//   - INSERT with RETURNING (save weekly report metadata)
//   - Reads from Divyansh's schema; does NOT modify core tables
// ============================================================
#pragma once
#include <string>
#include <vector>
#include <map>

namespace webnotifier {

// ── Data Structures ──────────────────────────────────────────────────────────

/**
 * @brief Uptime and performance statistics for one website over a date window.
 */
struct UptimeStats {
    int         website_id;
    std::string url;
    std::string name;
    int         total_checks;         ///< Total check results in the window
    int         up_count;             ///< Checks where status = 'UP'
    double      uptime_pct;           ///< 0.0 – 100.0
    long long   avg_response_ms;      ///< Average TTFB in milliseconds
    int         downtime_incidents;   ///< COUNT of consecutive DOWN events
    long long   downtime_minutes;     ///< Estimated downtime = incidents × avg_interval
    int         total_alerts;         ///< Alert rows linked to this website
};

/**
 * @brief Aggregated weekly report for a user, ready to export.
 */
struct WeeklyReport {
    int         user_id;
    std::string week_start;             ///< YYYY-MM-DD
    std::string week_end;               ///< YYYY-MM-DD
    std::vector<UptimeStats> website_stats;
    double      overall_uptime_pct;     ///< Mean of all website uptime percentages
    long long   total_downtime_min;     ///< Sum across all websites
    int         total_alerts;           ///< Sum across all websites
    std::string report_path;            ///< Absolute path to generated PDF (or CSV)
    std::string csv_path;               ///< Absolute path to generated CSV
    std::string generated_at;           ///< ISO8601 timestamp
};

// ── Analytics Engine Class ───────────────────────────────────────────────────

/**
 * @brief Data Access Object (DAO) that runs analytical SQL queries
 *        against the monitoring_results table and generates reports.
 *
 * All public methods are safe to call concurrently from different threads
 * (each call acquires its own PGconn from the connection string).
 *
 * Security: Credentials are sourced exclusively from environment variables.
 *           All queries use PQexecParams to prevent SQL injection.
 */
class AnalyticsEngine {
public:
    /**
     * @param db_conn_string  libpq connection string (built from env vars).
     * @param reports_dir     Directory where CSV/PDF files are written.
     *                        Defaults to "reports/". Created if it does not exist.
     */
    explicit AnalyticsEngine(const std::string& db_conn_string,
                             const std::string& reports_dir = "reports/");
    ~AnalyticsEngine();

    // Non-copyable (owns a PGconn*)
    AnalyticsEngine(const AnalyticsEngine&)            = delete;
    AnalyticsEngine& operator=(const AnalyticsEngine&) = delete;

    // ── Task 1: Analytical SQL Queries ──────────────────────────────────────

    /**
     * @brief 7-day (or N-day) uptime & response stats for one website.
     *
     * SQL: COUNT, SUM(CASE WHEN status='UP'), AVG(response_time_ms)
     *       WHERE checked_at >= NOW() - INTERVAL 'N days'
     */
    UptimeStats get_uptime_stats(int website_id, int days = 7);

    /**
     * @brief Stats for ALL websites owned by a user (JOIN + GROUP BY).
     *
     * SQL: LEFT JOIN websites + monitoring_results + alerts,
     *      GROUP BY w.id, w.url
     */
    std::vector<UptimeStats> get_all_stats(int user_id, int days = 7);

    /**
     * @brief Count of downtime incidents for one website in the window.
     *
     * An "incident" is a row where status = 'DOWN'.
     * SQL: COUNT(*) WHERE status = 'DOWN'
     */
    int get_downtime_incidents(int website_id, int days = 7);

    /**
     * @brief Average response time (TTFB) for one website over the window.
     *
     * SQL: AVG(response_time_ms) WHERE checked_at >= NOW() - INTERVAL 'N days'
     */
    double get_avg_response_time(int website_id, int days = 7);

    // ── Task 3: Report Generation ────────────────────────────────────────────

    /**
     * @brief Build a WeeklyReport, export CSV + PDF, dispatch email, save to DB.
     * @param user_id     User whose websites are aggregated.
     * @param week_start  YYYY-MM-DD start of the reporting window.
     * @param week_end    YYYY-MM-DD end of the reporting window.
     */
    WeeklyReport generate_weekly_report(int user_id,
                                         const std::string& week_start,
                                         const std::string& week_end);

    /**
     * @brief Export stats to CSV.
     * @return Absolute path to the generated CSV file.
     */
    std::string export_to_csv(const WeeklyReport& report);

    /**
     * @brief Generate HTML report, then shell out to wkhtmltopdf to produce PDF.
     * @return Absolute path to PDF, or empty string if wkhtmltopdf not available.
     */
    std::string export_to_pdf(const WeeklyReport& report);

    /**
     * @brief Generate JSON summary string for the dashboard API.
     */
    std::string generate_summary_json(int user_id);

    /**
     * @brief INSERT INTO weekly_reports with the report metadata.
     */
    bool save_report_to_db(const WeeklyReport& report);

    /**
     * @brief POST report path to Divyansh's internal email API.
     * Calls POST /api/internal/send-report via libcurl.
     * @return true if the API returned HTTP 200/201/202.
     */
    bool dispatch_report_email(const WeeklyReport& report);

private:
    // ── DB helpers ───────────────────────────────────────────
    /**
     * @brief Execute a plain SELECT and return rows as string maps.
     *        All values are returned as std::string (NULL → empty string).
     */
    std::vector<std::map<std::string,std::string>>
        query(const std::string& sql);

    /**
     * @brief Execute a parameterized SELECT (prevents SQL injection).
     * @param sql    SQL with $1, $2, … placeholders.
     * @param params Parameter values as strings.
     */
    std::vector<std::map<std::string,std::string>>
        query_params(const std::string& sql,
                     const std::vector<std::string>& params);

    /**
     * @brief Execute a non-SELECT statement (INSERT/UPDATE).
     * @return true on PGRES_COMMAND_OK or PGRES_TUPLES_OK.
     */
    bool exec_params(const std::string& sql,
                     const std::vector<std::string>& params);

    std::string current_timestamp() const;

    // ── HTML template builder ─────────────────────────────────
    std::string build_html_report(const WeeklyReport& report) const;

    // ── Data members ──────────────────────────────────────────
    std::string db_conn_string_;
    void*       conn_;            ///< PGconn* (opaque to keep header clean)
    std::string reports_dir_;
};

} // namespace webnotifier
