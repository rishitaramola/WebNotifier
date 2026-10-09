-- ============================================================
-- 006_check_requests.sql
-- On-demand "check now" signal table.
--
-- The frontend (on dashboard open) asks the backend to insert a row here.
-- The C++ scheduler polls this table every few seconds; when it finds an
-- un-processed row it immediately enqueues all active websites (bypassing the
-- normal 60s interval) and flips the row to processed = TRUE.
-- ============================================================
CREATE TABLE IF NOT EXISTS check_requests (
    id           SERIAL PRIMARY KEY,
    requested_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    requested_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    processed    BOOLEAN     NOT NULL DEFAULT FALSE
);

-- Fast lookup of pending requests by the scheduler.
CREATE INDEX IF NOT EXISTS idx_check_requests_pending
    ON check_requests (requested_at)
    WHERE processed = FALSE;
