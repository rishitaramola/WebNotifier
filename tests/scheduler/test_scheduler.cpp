// ============================================================
// tests/scheduler/test_scheduler.cpp
// Unit Tests — Scheduler & Analytics Module
// Owner: Rishita Ramola
//
// Framework: No external test library required.
//   Uses a lightweight hand-rolled assert macro so the tests
//   compile with a plain C++17 toolchain and no GoogleTest dep.
//
// What is tested:
//   1. FixedIntervalStrategy boundary conditions
//   2. AdaptiveStrategy doubles frequency on high failure rate
//   3. Scheduler::add_website() and is_running() lifecycle
//   4. AnalyticsEngine with ZERO data (no crash, no divide-by-zero)
//   5. WeeklyReport empty-data "polite" message path
// ============================================================
#include "../../scheduler/include/scheduling_strategy.h"
#include "../../scheduler/include/scheduler.h"
#include "../../analytics/include/analytics_engine.h"
#include "../../queue/include/task_queue.h"

#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// Minimal test harness
// ─────────────────────────────────────────────────────────────────────────────
static int g_pass = 0, g_fail = 0;

#define TEST(name) void name()
#define RUN(name)  do { \
    std::cout << "  [ RUN ] " #name "\n"; \
    try { name(); std::cout << "  [ OK  ] " #name "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
        std::cout << "  [FAIL ] " #name " — " << e.what() << "\n"; ++g_fail; } \
    catch (...) { \
        std::cout << "  [FAIL ] " #name " — unknown exception\n"; ++g_fail; } \
} while(0)

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) throw std::runtime_error("ASSERT_TRUE failed: " #cond); } while(0)
#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { \
        std::ostringstream _ss; \
        _ss << "ASSERT_EQ failed: " #a " (" << (a) << ") != " #b " (" << (b) << ")"; \
        throw std::runtime_error(_ss.str()); } } while(0)
#define ASSERT_GE(a, b) \
    do { if (!((a) >= (b))) { \
        std::ostringstream _ss; \
        _ss << "ASSERT_GE failed: " #a " (" << (a) << ") < " #b " (" << (b) << ")"; \
        throw std::runtime_error(_ss.str()); } } while(0)

#include <sstream>   // for ASSERT_EQ / ASSERT_GE stringification

// ─────────────────────────────────────────────────────────────────────────────
// Test Suite 1: FixedIntervalStrategy
// ─────────────────────────────────────────────────────────────────────────────
TEST(test_fixed_strategy_not_due) {
    webnotifier::FixedIntervalStrategy s;
    long long now        = 1700000000LL;  // arbitrary epoch
    long long last       = now - 60;      // checked 1 minute ago
    int       interval   = 5;            // check every 5 minutes
    // Only 60s elapsed, need 300s — should NOT check
    ASSERT_TRUE(!s.should_check(1, last, interval, now));
}

TEST(test_fixed_strategy_exactly_due) {
    webnotifier::FixedIntervalStrategy s;
    long long now      = 1700000300LL;
    long long last     = now - 300;      // exactly 5 min ago
    ASSERT_TRUE(s.should_check(1, last, 5, now));
}

TEST(test_fixed_strategy_overdue) {
    webnotifier::FixedIntervalStrategy s;
    long long now      = 1700001000LL;
    long long last     = now - 700;      // 11+ min ago, interval = 5
    ASSERT_TRUE(s.should_check(1, last, 5, now));
}

TEST(test_fixed_strategy_never_checked) {
    // last_checked = 0 (never checked) — should always be due
    webnotifier::FixedIntervalStrategy s;
    long long now = 1700000000LL;
    ASSERT_TRUE(s.should_check(1, 0LL, 5, now));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Suite 2: AdaptiveStrategy
// ─────────────────────────────────────────────────────────────────────────────
TEST(test_adaptive_healthy_site_uses_full_interval) {
    // failure_rate = 0.0 → effective interval = full 5 min
    webnotifier::AdaptiveStrategy s([](int) { return 0.0; });
    long long now  = 1700000300LL;
    long long last = now - 250;   // only 250s elapsed, need 300s
    ASSERT_TRUE(!s.should_check(1, last, 5, now));
}

TEST(test_adaptive_failing_site_halves_interval) {
    // failure_rate = 0.8 → effective interval = 150s (5min / 2)
    webnotifier::AdaptiveStrategy s([](int) { return 0.8; });
    long long now  = 1700000300LL;
    long long last = now - 160;   // 160s elapsed > 150s — should check
    ASSERT_TRUE(s.should_check(1, last, 5, now));
}

TEST(test_adaptive_default_no_fn_acts_like_fixed) {
    // No fn injected → always 0.0 failure rate → fixed interval behaviour
    webnotifier::AdaptiveStrategy s;
    long long now  = 1700000300LL;
    long long last = now - 300;
    ASSERT_TRUE(s.should_check(1, last, 5, now));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Suite 3: Scheduler lifecycle (no DB required)
// ─────────────────────────────────────────────────────────────────────────────
TEST(test_scheduler_not_running_before_start) {
    webnotifier::TaskQueue q(10);
    webnotifier::Scheduler sched(q);
    ASSERT_TRUE(!sched.is_running());
}

TEST(test_scheduler_add_website_does_not_crash) {
    webnotifier::TaskQueue q(10);
    webnotifier::Scheduler sched(q);
    webnotifier::WebsiteConfig cfg;
    cfg.website_id       = 42;
    cfg.url              = "https://example.com";
    cfg.keyword          = "Example";
    cfg.timeout_sec      = 10;
    cfg.notify_email     = "test@example.com";
    cfg.check_interval_min = 5;
    cfg.is_active        = true;
    // add_website must not throw even without a DB connection
    sched.add_website(cfg);
    ASSERT_TRUE(true);  // reached here = no crash
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Suite 4: AnalyticsEngine — empty / no-DB safety
// ─────────────────────────────────────────────────────────────────────────────
TEST(test_analytics_uptime_stats_no_conn_returns_defaults) {
    // Intentionally bad connection string → conn_ = nullptr
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    // Must not throw, must return zero-initialised struct
    auto stats = engine.get_uptime_stats(999, 7);
    ASSERT_EQ(stats.website_id,  999);
    ASSERT_EQ(stats.total_checks, 0);
    ASSERT_EQ(stats.up_count,     0);
    // uptime_pct with zero checks must be 0.0 (no divide-by-zero)
    ASSERT_EQ(stats.uptime_pct,  0.0);
}

TEST(test_analytics_get_all_stats_no_conn_returns_empty) {
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    auto stats = engine.get_all_stats(1, 7);
    ASSERT_TRUE(stats.empty());
}

TEST(test_analytics_weekly_report_no_data_polite_message) {
    // With no DB, get_all_stats returns empty → generate_weekly_report
    // must NOT segfault or divide by zero, and must set overall_uptime_pct = 0.0
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    auto report = engine.generate_weekly_report(1, "2026-09-08", "2026-09-14");
    ASSERT_TRUE(report.website_stats.empty());
    ASSERT_EQ(report.overall_uptime_pct, 0.0);
    ASSERT_EQ(report.total_downtime_min, 0LL);
    ASSERT_EQ(report.total_alerts,       0);
}

TEST(test_analytics_export_csv_empty_report) {
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    webnotifier::WeeklyReport report;
    report.user_id      = 1;
    report.week_start   = "2026-09-08";
    report.week_end     = "2026-09-14";
    report.generated_at = "2026-09-14T23:59:00Z";
    // Empty website_stats → CSV must not crash
    std::string path = engine.export_to_csv(report);
    // Path may be empty if /tmp/… isn't writable in test env — just no crash
    ASSERT_TRUE(true);
}

TEST(test_analytics_downtime_incidents_no_conn) {
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    int incidents = engine.get_downtime_incidents(1, 7);
    ASSERT_EQ(incidents, 0);
}

TEST(test_analytics_avg_response_no_conn) {
    webnotifier::AnalyticsEngine engine("host=localhost port=1 dbname=nonexistent",
                                        "/tmp/webnotifier_test_reports/");
    double avg = engine.get_avg_response_time(1, 7);
    ASSERT_EQ(avg, 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "====================================================\n"
              << " WebNotifier — Unit Tests (Scheduler + Analytics)\n"
              << " Owner: Rishita Ramola\n"
              << "====================================================\n\n";

    std::cout << "--- FixedIntervalStrategy ---\n";
    RUN(test_fixed_strategy_not_due);
    RUN(test_fixed_strategy_exactly_due);
    RUN(test_fixed_strategy_overdue);
    RUN(test_fixed_strategy_never_checked);

    std::cout << "\n--- AdaptiveStrategy ---\n";
    RUN(test_adaptive_healthy_site_uses_full_interval);
    RUN(test_adaptive_failing_site_halves_interval);
    RUN(test_adaptive_default_no_fn_acts_like_fixed);

    std::cout << "\n--- Scheduler Lifecycle ---\n";
    RUN(test_scheduler_not_running_before_start);
    RUN(test_scheduler_add_website_does_not_crash);

    std::cout << "\n--- AnalyticsEngine (No-DB Safety) ---\n";
    RUN(test_analytics_uptime_stats_no_conn_returns_defaults);
    RUN(test_analytics_get_all_stats_no_conn_returns_empty);
    RUN(test_analytics_weekly_report_no_data_polite_message);
    RUN(test_analytics_export_csv_empty_report);
    RUN(test_analytics_downtime_incidents_no_conn);
    RUN(test_analytics_avg_response_no_conn);

    std::cout << "\n====================================================\n"
              << " Results: " << g_pass << " passed, " << g_fail << " failed\n"
              << "====================================================\n";
    return g_fail > 0 ? 1 : 0;
}
