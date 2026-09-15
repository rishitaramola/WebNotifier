-- ============================================================
-- database/migrations/004_analytics_views.sql
-- Extended Analytical Views for Rishita's Analytics Engine
-- Owner: Rishita Ramola
--
-- DBMS Concepts:
--   - Aggregation   (COUNT, AVG, SUM, ROUND)
--   - GROUP BY      (per-website aggregation)
--   - Rolling window(NOW() - INTERVAL '7 days')
--   - LEFT JOIN     (sites with zero checks still appear)
--   - NULLIF        (guards against divide-by-zero in uptime %)
--   - CTEs          (WITH clause for readability)
--
-- NOTE: These views are READ-ONLY additions.
--       They do NOT modify any of Divyansh's core tables.
--       Simran's dashboard views (001_create_views.sql) are preserved.
-- ============================================================

-- ── View 1: Downtime Incident Counts (last 7 days) ───────────────────────────
-- Counts the number of DOWN status rows per website.
-- "Incident" = one monitoring_results row where status = 'DOWN'.
-- DBMS Concept: Aggregation + WHERE filter on status
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_downtime_incidents_7d AS
SELECT
    mr.website_id,
    w.url,
    w.name,
    COUNT(*)                                  AS downtime_incidents,
    MAX(mr.checked_at)                        AS last_downtime_at
FROM monitoring_results mr
JOIN websites w ON mr.website_id = w.id
WHERE mr.status     = 'DOWN'
  AND mr.checked_at >= NOW() - INTERVAL '7 days'
GROUP BY mr.website_id, w.url, w.name;

-- ── View 2: Average Response Time / TTFB (last 7 days) ───────────────────────
-- Average Time to First Byte, excluding TIMEOUT results (outliers).
-- DBMS Concept: AVG aggregation + status filter
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_avg_response_7d AS
SELECT
    mr.website_id,
    w.url,
    w.name,
    ROUND(AVG(mr.response_time_ms), 0)        AS avg_response_ms,
    MIN(mr.response_time_ms)                  AS min_response_ms,
    MAX(mr.response_time_ms)                  AS max_response_ms,
    COUNT(*)                                  AS sample_count
FROM monitoring_results mr
JOIN websites w ON mr.website_id = w.id
WHERE mr.status     != 'TIMEOUT'
  AND mr.checked_at >= NOW() - INTERVAL '7 days'
GROUP BY mr.website_id, w.url, w.name;

-- ── View 3: Complete Weekly Summary (per-user, all websites) ─────────────────
-- Combines uptime %, downtime incidents, TTFB, and alert count.
-- Used by AnalyticsEngine::get_all_stats() as a pre-computed shortcut.
-- DBMS Concept: Multi-table JOIN + GROUP BY + NULLIF (divide-by-zero guard)
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_weekly_summary AS
WITH checks AS (
    SELECT
        website_id,
        COUNT(*)                                                   AS total_checks,
        SUM(CASE WHEN status = 'UP'   THEN 1 ELSE 0 END)          AS up_count,
        SUM(CASE WHEN status = 'DOWN' THEN 1 ELSE 0 END)          AS down_count,
        ROUND(
            100.0
            * SUM(CASE WHEN status = 'UP' THEN 1 ELSE 0 END)
            / NULLIF(COUNT(*), 0),
            2
        )                                                          AS uptime_pct,
        COALESCE(
            AVG(CASE WHEN status != 'TIMEOUT'
                     THEN response_time_ms END), 0
        )                                                          AS avg_response_ms
    FROM monitoring_results
    WHERE checked_at >= NOW() - INTERVAL '7 days'
    GROUP BY website_id
),
alert_counts AS (
    SELECT
        a.website_id,
        COUNT(*) AS alert_count
    FROM alerts a
    WHERE a.sent_at >= NOW() - INTERVAL '7 days'
    GROUP BY a.website_id
)
SELECT
    w.user_id,
    w.id                                                          AS website_id,
    w.url,
    w.name,
    w.check_interval_min,
    COALESCE(c.total_checks,    0)                                AS total_checks,
    COALESCE(c.up_count,        0)                                AS up_count,
    COALESCE(c.down_count,      0)                                AS downtime_incidents,
    COALESCE(c.uptime_pct,      0.00)                             AS uptime_pct,
    COALESCE(c.avg_response_ms, 0)                                AS avg_response_ms,
    COALESCE(c.down_count, 0) * w.check_interval_min              AS downtime_minutes,
    COALESCE(ac.alert_count,    0)                                AS total_alerts
FROM websites w
LEFT JOIN checks       c  ON c.website_id  = w.id
LEFT JOIN alert_counts ac ON ac.website_id = w.id
WHERE w.is_active = TRUE;

-- ── View 4: Per-User Aggregate (for executive summary) ───────────────────────
-- Single-row summary per user for the executive summary box in the PDF.
-- DBMS Concept: Nested aggregation over v_weekly_summary
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_user_weekly_aggregate AS
SELECT
    user_id,
    COUNT(website_id)                                             AS monitored_sites,
    ROUND(AVG(uptime_pct), 2)                                     AS overall_uptime_pct,
    SUM(downtime_minutes)                                         AS total_downtime_min,
    SUM(total_alerts)                                             AS total_alerts,
    SUM(total_checks)                                             AS total_checks
FROM v_weekly_summary
GROUP BY user_id;
