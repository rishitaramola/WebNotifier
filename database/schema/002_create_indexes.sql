-- ============================================================
-- WebNotifier - Index Definitions
-- Improves query performance for common access patterns
-- ============================================================

-- Users lookup by email (login)
CREATE INDEX IF NOT EXISTS idx_users_email        ON users(email);

-- Websites by owner
CREATE INDEX IF NOT EXISTS idx_websites_user_id   ON websites(user_id);
CREATE INDEX IF NOT EXISTS idx_websites_active     ON websites(is_active);

-- Monitoring results: time-series queries
CREATE INDEX IF NOT EXISTS idx_results_website_id ON monitoring_results(website_id);
CREATE INDEX IF NOT EXISTS idx_results_checked_at ON monitoring_results(checked_at DESC);
CREATE INDEX IF NOT EXISTS idx_results_status      ON monitoring_results(status);

-- Jobs: scheduler queries
CREATE INDEX IF NOT EXISTS idx_jobs_scheduled_at  ON monitoring_jobs(scheduled_at);
CREATE INDEX IF NOT EXISTS idx_jobs_status         ON monitoring_jobs(status);

-- Alerts
CREATE INDEX IF NOT EXISTS idx_alerts_website_id  ON alerts(website_id);
CREATE INDEX IF NOT EXISTS idx_alerts_sent_at      ON alerts(sent_at DESC);

-- Reports
CREATE INDEX IF NOT EXISTS idx_reports_user_id    ON weekly_reports(user_id);
CREATE INDEX IF NOT EXISTS idx_reports_week_start ON weekly_reports(week_start DESC);

-- System metrics (time-series)
CREATE INDEX IF NOT EXISTS idx_metrics_recorded_at ON system_metrics(recorded_at DESC);
