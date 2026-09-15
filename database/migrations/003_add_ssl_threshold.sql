-- ============================================================
-- WebNotifier - Migration: Add SSL Alert Threshold
-- Migration #003
-- ============================================================

ALTER TABLE websites
    ADD COLUMN IF NOT EXISTS ssl_alert_days INTEGER NOT NULL DEFAULT 30;

ALTER TABLE monitoring_results
    ADD COLUMN IF NOT EXISTS is_ssl_valid BOOLEAN DEFAULT NULL;

COMMENT ON COLUMN websites.ssl_alert_days IS 'Alert when SSL expires within this many days';
COMMENT ON COLUMN monitoring_results.is_ssl_valid IS 'True if SSL cert is valid at time of check';
