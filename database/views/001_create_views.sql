-- ============================================================
-- WebNotifier - Database Views
-- Pre-computed queries for dashboard and analytics
-- ============================================================

-- View: latest status of each website
CREATE OR REPLACE VIEW v_website_current_status AS
SELECT
    w.id           AS website_id,
    w.user_id,
    w.url,
    w.name,
    mr.status,
    mr.http_code,
    mr.response_time_ms,
    mr.checked_at  AS last_checked,
    mr.ssl_expiry_days
FROM websites w
LEFT JOIN LATERAL (
    SELECT * FROM monitoring_results
    WHERE website_id = w.id
    ORDER BY checked_at DESC
    LIMIT 1
) mr ON TRUE;

-- View: uptime percentage per website (last 7 days)
CREATE OR REPLACE VIEW v_uptime_last7days AS
SELECT
    website_id,
    COUNT(*)                                            AS total_checks,
    SUM(CASE WHEN status = 'UP' THEN 1 ELSE 0 END)     AS up_count,
    ROUND(
        100.0 * SUM(CASE WHEN status = 'UP' THEN 1 ELSE 0 END) / COUNT(*),
        2
    )                                                   AS uptime_pct,
    AVG(response_time_ms)                               AS avg_response_ms
FROM monitoring_results
WHERE checked_at >= NOW() - INTERVAL '7 days'
GROUP BY website_id;

-- View: alert summary per user
CREATE OR REPLACE VIEW v_user_alert_summary AS
SELECT
    w.user_id,
    COUNT(a.id)   AS total_alerts,
    MAX(a.sent_at) AS last_alert_at
FROM alerts a
JOIN websites w ON a.website_id = w.id
GROUP BY w.user_id;

-- View: pending monitoring jobs
CREATE OR REPLACE VIEW v_pending_jobs AS
SELECT
    mj.id            AS job_id,
    mj.website_id,
    w.url,
    w.keyword,
    w.timeout_seconds,
    w.notify_email,
    mj.scheduled_at
FROM monitoring_jobs mj
JOIN websites w ON mj.website_id = w.id
WHERE mj.status = 'pending'
  AND mj.scheduled_at <= NOW()
ORDER BY mj.scheduled_at ASC;
