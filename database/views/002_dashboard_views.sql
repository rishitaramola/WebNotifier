-- ============================================================
-- database/views/002_dashboard_views.sql
-- Dashboard-Specific SQL Views
-- Owner: Simran Negi
--
-- DBMS Concepts Demonstrated:
--   - Views        : pre-computed queries for instant dashboard load
--   - JOIN         : combining websites + monitoring_results + alerts
--   - Aggregation  : COUNT, AVG, ROUND, MAX
--   - CASE WHEN    : conditional logic inside SQL
--   - LATERAL JOIN : most-recent-row-per-group pattern
--   - COALESCE     : NULL-safe defaults
--   - Rolling window: NOW() - INTERVAL 'N days'
--
-- Purpose:
--   Instead of the frontend triggering heavy raw queries, these
--   views pre-filter and format data at the schema level.
--   Divyansh's API simply does: SELECT * FROM <view> WHERE user_id = $1
-- ============================================================


-- ── View 1: Dashboard Website Cards ──────────────────────────────────────────
-- One row per website with ALL data needed to render a status card:
--   current status, uptime %, avg response, SSL days, alert count.
--
-- DBMS Concepts: Multi-table JOIN, LEFT JOIN (sites with zero checks
--   still appear), LATERAL for latest result, NULLIF divide-by-zero guard.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_dashboard_website_cards AS
SELECT
    w.id                                                        AS website_id,
    w.user_id,
    w.url,
    w.name,
    w.check_interval_min,
    w.is_active,

    -- Latest check result (LATERAL = most recent row per website)
    COALESCE(latest.status,           'UNKNOWN')                AS current_status,
    COALESCE(latest.http_code,        0)                        AS last_http_code,
    COALESCE(latest.response_time_ms, 0)                        AS last_response_ms,
    COALESCE(latest.ssl_expiry_days,  -1)                       AS ssl_expiry_days,
    latest.checked_at                                           AS last_checked_at,

    -- 7-day uptime percentage
    COALESCE(
        ROUND(
            100.0
            * SUM(CASE WHEN mr.status = 'UP' THEN 1 ELSE 0 END)
            / NULLIF(COUNT(mr.id), 0),
            2
        ),
        0.00
    )                                                           AS uptime_pct_7d,

    -- 7-day average response time (ms), excluding timeouts
    COALESCE(
        ROUND(AVG(CASE WHEN mr.status != 'TIMEOUT'
                       THEN mr.response_time_ms END), 0),
        0
    )                                                           AS avg_response_ms_7d,

    -- Total checks in last 7 days
    COUNT(mr.id)                                                AS total_checks_7d,

    -- Downtime incidents in last 7 days
    SUM(CASE WHEN mr.status = 'DOWN' THEN 1 ELSE 0 END)        AS downtime_incidents_7d,

    -- Alert count in last 7 days
    COALESCE(ac.alert_count, 0)                                 AS alerts_7d,

    -- SSL warning flag: TRUE if SSL expires within ssl_alert_days threshold
    CASE
        WHEN latest.ssl_expiry_days IS NOT NULL
             AND latest.ssl_expiry_days >= 0
             AND latest.ssl_expiry_days <= w.ssl_alert_days
        THEN TRUE
        ELSE FALSE
    END                                                         AS ssl_warning

FROM websites w

-- Most recent monitoring result per website (LATERAL JOIN pattern)
LEFT JOIN LATERAL (
    SELECT status, http_code, response_time_ms, ssl_expiry_days, checked_at
    FROM   monitoring_results
    WHERE  website_id = w.id
    ORDER  BY checked_at DESC
    LIMIT  1
) latest ON TRUE

-- All results in the last 7 days (for aggregation)
LEFT JOIN monitoring_results mr
       ON mr.website_id = w.id
      AND mr.checked_at >= NOW() - INTERVAL '7 days'

-- Alert count per website in last 7 days
LEFT JOIN (
    SELECT   website_id, COUNT(*) AS alert_count
    FROM     alerts
    WHERE    sent_at >= NOW() - INTERVAL '7 days'
    GROUP BY website_id
) ac ON ac.website_id = w.id

WHERE w.is_active = TRUE
GROUP BY
    w.id, w.user_id, w.url, w.name, w.check_interval_min,
    w.is_active, w.ssl_alert_days,
    latest.status, latest.http_code, latest.response_time_ms,
    latest.ssl_expiry_days, latest.checked_at,
    ac.alert_count;


-- ── View 2: Recent Alerts Feed ────────────────────────────────────────────────
-- Last 50 alerts across all websites, joined with website name and user.
-- Used by the Alerts panel on the dashboard.
--
-- DBMS Concepts: JOIN, ORDER BY DESC, LIMIT via application layer.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_recent_alerts AS
SELECT
    a.id                                                        AS alert_id,
    a.website_id,
    w.user_id,
    w.url,
    w.name                                                      AS website_name,
    a.alert_type,
    a.message,
    a.sent_to,
    a.sent_at,
    a.acknowledged,
    -- Human-readable label for UI badge colour
    CASE a.alert_type
        WHEN 'DOWN'            THEN 'danger'
        WHEN 'SSL_EXPIRY'      THEN 'warning'
        WHEN 'KEYWORD_MISSING' THEN 'warning'
        WHEN 'TIMEOUT'         THEN 'secondary'
        ELSE                        'info'
    END                                                         AS badge_class
FROM alerts a
JOIN websites w ON a.website_id = w.id
ORDER BY a.sent_at DESC;


-- ── View 3: SSL Expiry Warnings ───────────────────────────────────────────────
-- Websites whose SSL certificate expires within their configured threshold.
-- Sorted by most urgent (fewest days remaining) first.
--
-- DBMS Concepts: WHERE with computed column, ORDER BY ASC for urgency.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_ssl_expiry_warnings AS
SELECT
    w.id                                                        AS website_id,
    w.user_id,
    w.url,
    w.name,
    w.ssl_alert_days                                            AS alert_threshold_days,
    latest.ssl_expiry_days                                      AS days_remaining,
    latest.checked_at                                           AS last_checked_at,
    CASE
        WHEN latest.ssl_expiry_days <= 7  THEN 'critical'
        WHEN latest.ssl_expiry_days <= 14 THEN 'high'
        ELSE                                   'medium'
    END                                                         AS urgency
FROM websites w
JOIN LATERAL (
    SELECT ssl_expiry_days, checked_at
    FROM   monitoring_results
    WHERE  website_id    = w.id
      AND  ssl_expiry_days IS NOT NULL
      AND  ssl_expiry_days >= 0
    ORDER  BY checked_at DESC
    LIMIT  1
) latest ON TRUE
WHERE latest.ssl_expiry_days <= w.ssl_alert_days
  AND w.is_active = TRUE
ORDER BY latest.ssl_expiry_days ASC;


-- ── View 4: Live System Health (latest metrics snapshot) ─────────────────────
-- Single-row view of the most recent system_metrics entry.
-- Used by the dashboard's Server Health widget.
--
-- DBMS Concepts: Subquery with ORDER BY + LIMIT 1, scalar aggregation.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_system_health_latest AS
SELECT
    id,
    cpu_usage_pct,
    mem_usage_pct,
    active_threads,
    queue_depth,
    recorded_at,
    -- Health labels for UI colour coding
    CASE
        WHEN cpu_usage_pct >= 90 THEN 'critical'
        WHEN cpu_usage_pct >= 70 THEN 'warning'
        ELSE                          'healthy'
    END                                                         AS cpu_status,
    CASE
        WHEN mem_usage_pct >= 90 THEN 'critical'
        WHEN mem_usage_pct >= 70 THEN 'warning'
        ELSE                          'healthy'
    END                                                         AS mem_status
FROM system_metrics
ORDER BY recorded_at DESC
LIMIT 1;


-- ── View 5: System Metrics History (last 60 readings) ────────────────────────
-- Time-series data for the CPU/RAM sparkline chart on the dashboard.
--
-- DBMS Concepts: ORDER BY time DESC, rolling history window.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_system_metrics_history AS
SELECT
    id,
    cpu_usage_pct,
    mem_usage_pct,
    active_threads,
    queue_depth,
    recorded_at
FROM system_metrics
ORDER BY recorded_at DESC
LIMIT 60;


-- ── View 6: User Dashboard Summary ───────────────────────────────────────────
-- Single summary row per user: total sites, how many are UP/DOWN,
-- overall uptime %, total alerts today. Used for the top stats bar.
--
-- DBMS Concepts: Nested aggregation, CASE WHEN inside COUNT,
--   ROUND + NULLIF divide-by-zero guard.
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW v_user_dashboard_summary AS
SELECT
    w.user_id,
    COUNT(DISTINCT w.id)                                        AS total_sites,

    -- Sites currently UP (based on latest check)
    COUNT(DISTINCT CASE
        WHEN latest.status = 'UP' THEN w.id
    END)                                                        AS sites_up,

    -- Sites currently DOWN
    COUNT(DISTINCT CASE
        WHEN latest.status = 'DOWN' THEN w.id
    END)                                                        AS sites_down,

    -- Sites with unknown / no data
    COUNT(DISTINCT CASE
        WHEN latest.status IS NULL OR latest.status = 'UNKNOWN' THEN w.id
    END)                                                        AS sites_unknown,

    -- Overall average uptime across all sites (last 7 days)
    COALESCE(
        ROUND(AVG(
            CASE WHEN agg.total_checks > 0
                 THEN agg.uptime_pct
            END
        ), 2),
        0.00
    )                                                           AS overall_uptime_pct,

    -- Alerts fired in the last 24 hours
    COALESCE(SUM(daily_alerts.alert_count), 0)                  AS alerts_last_24h,

    -- Alerts fired in the last 7 days
    COALESCE(SUM(weekly_alerts.alert_count), 0)                 AS alerts_last_7d

FROM websites w

LEFT JOIN LATERAL (
    SELECT status
    FROM   monitoring_results
    WHERE  website_id = w.id
    ORDER  BY checked_at DESC
    LIMIT  1
) latest ON TRUE

LEFT JOIN (
    SELECT
        website_id,
        COUNT(*)                                                AS total_checks,
        ROUND(
            100.0
            * SUM(CASE WHEN status = 'UP' THEN 1 ELSE 0 END)
            / NULLIF(COUNT(*), 0),
            2
        )                                                       AS uptime_pct
    FROM monitoring_results
    WHERE checked_at >= NOW() - INTERVAL '7 days'
    GROUP BY website_id
) agg ON agg.website_id = w.id

LEFT JOIN (
    SELECT   website_id, COUNT(*) AS alert_count
    FROM     alerts
    WHERE    sent_at >= NOW() - INTERVAL '1 day'
    GROUP BY website_id
) daily_alerts ON daily_alerts.website_id = w.id

LEFT JOIN (
    SELECT   website_id, COUNT(*) AS alert_count
    FROM     alerts
    WHERE    sent_at >= NOW() - INTERVAL '7 days'
    GROUP BY website_id
) weekly_alerts ON weekly_alerts.website_id = w.id

WHERE w.is_active = TRUE
GROUP BY w.user_id;
