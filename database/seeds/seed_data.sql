-- ============================================================
-- WebNotifier - Sample Seed Data
-- For development and testing
-- ============================================================

-- Sample Users
INSERT INTO users (username, email, password_hash, role) VALUES
('rishita',   'rishita@example.com',   '$2b$12$hashed_password_here_1', 'admin'),
('shivank',   'shivank@example.com',   '$2b$12$hashed_password_here_2', 'user'),
('divyansh',  'divyansh@example.com',  '$2b$12$hashed_password_here_3', 'user'),
('simran',    'simran@example.com',    '$2b$12$hashed_password_here_4', 'user');

-- Sample Websites to Monitor
INSERT INTO websites (user_id, url, name, check_interval_min, keyword, timeout_seconds, notify_email) VALUES
(1, 'https://google.com',       'Google',           5,  'Search',    30, 'rishita@example.com'),
(1, 'https://github.com',       'GitHub',           10, 'GitHub',    30, 'rishita@example.com'),
(2, 'https://stackoverflow.com','Stack Overflow',   15, 'Questions', 30, 'shivank@example.com'),
(3, 'https://wikipedia.org',    'Wikipedia',        10, 'Free',      30, 'divyansh@example.com'),
(4, 'https://openai.com',       'OpenAI',           5,  'ChatGPT',   30, 'simran@example.com'),
(1, 'https://example-down.xyz', 'Test Down Site',   5,  NULL,        10, 'rishita@example.com');

-- Sample Monitoring Results
INSERT INTO monitoring_results (website_id, status, http_code, response_time_ms, keyword_found, ssl_expiry_days, checked_at) VALUES
(1, 'UP',      200,  120,  TRUE,  365, NOW() - INTERVAL '1 hour'),
(1, 'UP',      200,  135,  TRUE,  365, NOW() - INTERVAL '2 hours'),
(1, 'DOWN',    503,  NULL, FALSE,  365, NOW() - INTERVAL '3 hours'),
(2, 'UP',      200,  240,  TRUE,  200, NOW() - INTERVAL '1 hour'),
(3, 'UP',      200,  320,  TRUE,  180, NOW() - INTERVAL '30 minutes'),
(4, 'TIMEOUT', NULL, NULL, FALSE, 90,  NOW() - INTERVAL '45 minutes'),
(5, 'UP',      200,  450,  TRUE,  120, NOW() - INTERVAL '15 minutes'),
(6, 'DOWN',    NULL, NULL, FALSE, NULL,NOW() - INTERVAL '10 minutes');

-- Sample Alerts
INSERT INTO alerts (website_id, result_id, alert_type, message, sent_to) VALUES
(1, 3, 'DOWN',    'google.com returned HTTP 503',          'rishita@example.com'),
(4, 6, 'TIMEOUT', 'wikipedia.org timed out after 30s',     'divyansh@example.com'),
(6, 8, 'DOWN',    'example-down.xyz is unreachable',       'rishita@example.com');

-- Sample System Metrics
INSERT INTO system_metrics (cpu_usage_pct, mem_usage_pct, active_threads, queue_depth) VALUES
(12.5, 34.2, 8, 3),
(15.0, 35.1, 8, 0),
(45.2, 38.9, 8, 12),
(22.1, 36.0, 8, 5);
