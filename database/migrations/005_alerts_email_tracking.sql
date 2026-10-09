-- ============================================================
-- 005_alerts_email_tracking.sql
-- Fixes the misleading alerts.sent_at semantics.
--
-- BEFORE: sent_at had DEFAULT now(), so every alert looked "emailed"
--         even though no email was ever dispatched.
-- AFTER:  created_at  = when the alert was detected/recorded (real).
--         sent_at     = when an email was ACTUALLY dispatched (NULL until then).
--         sent_to     = the address the email actually went to.
-- ============================================================

-- 1. Add a true creation timestamp, backfilled from the old sent_at.
ALTER TABLE alerts ADD COLUMN IF NOT EXISTS created_at TIMESTAMP;
UPDATE alerts SET created_at = sent_at WHERE created_at IS NULL;
ALTER TABLE alerts ALTER COLUMN created_at SET DEFAULT NOW();
ALTER TABLE alerts ALTER COLUMN created_at SET NOT NULL;

-- 2. sent_at now means "email dispatched at" — remove the auto default and
--    allow NULL (it was NOT NULL to support the old default).
ALTER TABLE alerts ALTER COLUMN sent_at DROP DEFAULT;
ALTER TABLE alerts ALTER COLUMN sent_at DROP NOT NULL;

-- 3. Existing rows were never actually emailed — clear the false markers.
--    (They keep their created_at, so history is preserved; the email poller
--     ignores anything older than its lookback window, so no backlog blast.)
UPDATE alerts SET sent_at = NULL, sent_to = NULL;

-- 4. Index to let the email poller find un-sent alerts quickly.
CREATE INDEX IF NOT EXISTS idx_alerts_unsent
    ON alerts (created_at)
    WHERE sent_at IS NULL;

-- 5. Dashboard view: order by real detection time, expose created_at.
--    (DROP first: inserting a column mid-list is not allowed by REPLACE.)
DROP VIEW IF EXISTS v_recent_alerts;
CREATE VIEW v_recent_alerts AS
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
    a.created_at,
    a.acknowledged,
    CASE a.alert_type
        WHEN 'DOWN'            THEN 'danger'
        WHEN 'SSL_EXPIRY'      THEN 'warning'
        WHEN 'KEYWORD_MISSING' THEN 'warning'
        WHEN 'TIMEOUT'         THEN 'secondary'
        ELSE                        'info'
    END                                                         AS badge_class
FROM alerts a
JOIN websites w ON a.website_id = w.id
ORDER BY a.created_at DESC;
