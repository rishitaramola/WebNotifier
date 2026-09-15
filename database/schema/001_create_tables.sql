-- ============================================================
-- WebNotifier Database Schema
-- Team CodeSync | PostgreSQL
-- ============================================================

-- Enable UUID extension
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- ============================================================
-- TABLE: users
-- Stores registered users of the platform
-- ============================================================
CREATE TABLE IF NOT EXISTS users (
    id              SERIAL PRIMARY KEY,
    username        VARCHAR(50)  NOT NULL UNIQUE,
    email           VARCHAR(100) NOT NULL UNIQUE,
    password_hash   VARCHAR(255) NOT NULL,
    is_active       BOOLEAN      NOT NULL DEFAULT TRUE,
    role            VARCHAR(20)  NOT NULL DEFAULT 'user'
                        CHECK (role IN ('admin', 'user')),
    created_at      TIMESTAMP    NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMP    NOT NULL DEFAULT NOW()
);

-- ============================================================
-- TABLE: websites
-- Stores URLs submitted by users for monitoring
-- ============================================================
CREATE TABLE IF NOT EXISTS websites (
    id                  SERIAL PRIMARY KEY,
    user_id             INTEGER      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    url                 VARCHAR(2048) NOT NULL,
    name                VARCHAR(255) NOT NULL,
    check_interval_min  INTEGER      NOT NULL DEFAULT 5
                            CHECK (check_interval_min >= 1),
    keyword             VARCHAR(255),          -- optional keyword to search in response
    timeout_seconds     INTEGER      NOT NULL DEFAULT 30,
    is_active           BOOLEAN      NOT NULL DEFAULT TRUE,
    notify_email        VARCHAR(100),
    created_at          TIMESTAMP    NOT NULL DEFAULT NOW(),
    updated_at          TIMESTAMP    NOT NULL DEFAULT NOW()
);

-- ============================================================
-- TABLE: monitoring_jobs
-- Tracks scheduled jobs for each website
-- ============================================================
CREATE TABLE IF NOT EXISTS monitoring_jobs (
    id              SERIAL PRIMARY KEY,
    website_id      INTEGER   NOT NULL REFERENCES websites(id) ON DELETE CASCADE,
    scheduled_at    TIMESTAMP NOT NULL,
    executed_at     TIMESTAMP,
    status          VARCHAR(20) NOT NULL DEFAULT 'pending'
                        CHECK (status IN ('pending', 'running', 'completed', 'failed')),
    created_at      TIMESTAMP NOT NULL DEFAULT NOW()
);

-- ============================================================
-- TABLE: monitoring_results
-- Stores the result of each website health check
-- ============================================================
CREATE TABLE IF NOT EXISTS monitoring_results (
    id                  SERIAL PRIMARY KEY,
    website_id          INTEGER      NOT NULL REFERENCES websites(id) ON DELETE CASCADE,
    job_id              INTEGER      REFERENCES monitoring_jobs(id) ON DELETE SET NULL,
    status              VARCHAR(10)  NOT NULL CHECK (status IN ('UP', 'DOWN', 'TIMEOUT', 'ERROR')),
    http_code           INTEGER,
    response_time_ms    BIGINT,
    keyword_found       BOOLEAN,
    ssl_expiry_days     INTEGER,
    error_message       TEXT,
    checked_at          TIMESTAMP    NOT NULL DEFAULT NOW()
);

-- ============================================================
-- TABLE: alerts
-- Stores alert records when a site goes DOWN
-- ============================================================
CREATE TABLE IF NOT EXISTS alerts (
    id              SERIAL PRIMARY KEY,
    website_id      INTEGER      NOT NULL REFERENCES websites(id) ON DELETE CASCADE,
    result_id       INTEGER      REFERENCES monitoring_results(id) ON DELETE SET NULL,
    alert_type      VARCHAR(30)  NOT NULL
                        CHECK (alert_type IN ('DOWN', 'SSL_EXPIRY', 'KEYWORD_MISSING', 'TIMEOUT')),
    message         TEXT         NOT NULL,
    sent_to         VARCHAR(100),
    sent_at         TIMESTAMP    NOT NULL DEFAULT NOW(),
    acknowledged    BOOLEAN      NOT NULL DEFAULT FALSE
);

-- ============================================================
-- TABLE: weekly_reports
-- Stores generated analytics reports
-- ============================================================
CREATE TABLE IF NOT EXISTS weekly_reports (
    id              SERIAL PRIMARY KEY,
    user_id         INTEGER      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    week_start      DATE         NOT NULL,
    week_end        DATE         NOT NULL,
    total_checks    INTEGER      NOT NULL DEFAULT 0,
    uptime_pct      DECIMAL(5,2),
    downtime_min    INTEGER      DEFAULT 0,
    avg_response_ms BIGINT,
    total_alerts    INTEGER      DEFAULT 0,
    report_path     VARCHAR(512),           -- path to generated PDF/CSV
    generated_at    TIMESTAMP    NOT NULL DEFAULT NOW()
);

-- ============================================================
-- TABLE: system_metrics
-- OS-level metrics collected by the background daemon
-- ============================================================
CREATE TABLE IF NOT EXISTS system_metrics (
    id              SERIAL PRIMARY KEY,
    cpu_usage_pct   DECIMAL(5,2),
    mem_usage_pct   DECIMAL(5,2),
    active_threads  INTEGER,
    queue_depth     INTEGER,
    recorded_at     TIMESTAMP    NOT NULL DEFAULT NOW()
);

-- ============================================================
-- CONSTRAINTS: prevent duplicate pending jobs
-- ============================================================
CREATE UNIQUE INDEX IF NOT EXISTS idx_pending_job
    ON monitoring_jobs(website_id, scheduled_at)
    WHERE status = 'pending';
