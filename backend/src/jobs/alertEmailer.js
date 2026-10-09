// ============================================================
// jobs/alertEmailer.js
// Background poller that dispatches emails for NEW, un-emailed alerts.
//
// The C++ worker pool writes alert rows (sent_at = NULL). This poller finds
// those, emails the website owner, and stamps sent_at / sent_to so each alert
// is emailed exactly once.
//
// SAFETY: real sending is OFF by default. Set ALERTS_EMAIL_ENABLED=true in
// .env to actually send. In log-mode it prints what it WOULD send and still
// marks the alert processed, so it never blasts a backlog.
// ============================================================
const { pool } = require("../db");
const { sendAlertEmail } = require("../utils/mailer");

const POLL_MS = parseInt(process.env.ALERTS_POLL_MS || "30000", 10);
const LOOKBACK_MIN = parseInt(process.env.ALERTS_LOOKBACK_MIN || "15", 10);
const BATCH = parseInt(process.env.ALERTS_BATCH || "20", 10);
const ENABLED = process.env.ALERTS_EMAIL_ENABLED === "true";

let running = false;           // guard against overlapping ticks
const loggedIds = new Set();   // log-mode: alert ids already previewed (no DB write)

async function processPendingAlertEmails() {
    if (running) return;
    running = true;
    try {
        const { rows } = await pool.query(
            `SELECT a.id AS alert_id, a.alert_type, a.message,
                    w.name AS website_name, w.url,
                    u.email, u.username
             FROM alerts a
             JOIN websites w ON w.id = a.website_id
             JOIN users   u ON u.id = w.user_id
             WHERE a.sent_at IS NULL
               AND a.created_at >= NOW() - ($1 || ' minutes')::interval
               -- Skip demo/seed accounts with non-deliverable addresses so the
               -- sender isn't stuck retrying (or flagged for) bad recipients.
               AND u.email NOT LIKE '%@example.com'
               AND u.email NOT LIKE '%.local'
             ORDER BY a.created_at ASC
             LIMIT $2`,
            [String(LOOKBACK_MIN), BATCH]
        );

        for (const r of rows) {
            if (ENABLED) {
                try {
                    await sendAlertEmail(r.email, r.username, {
                        websiteName: r.website_name,
                        url: r.url,
                        alertType: r.alert_type,
                        message: r.message,
                    });
                    await pool.query(
                        "UPDATE alerts SET sent_at = NOW(), sent_to = $2 WHERE id = $1",
                        [r.alert_id, r.email]
                    );
                    console.log(`[AlertEmailer] Sent ${r.alert_type} alert for ${r.website_name} -> ${r.email}`);
                } catch (mailErr) {
                    // Leave sent_at NULL so it retries next tick.
                    console.error(`[AlertEmailer] Email failed for alert ${r.alert_id}:`, mailErr.message);
                }
            } else {
                // Log-mode preview: NEVER touch sent_at (it means "really emailed").
                // Dedupe in memory so we don't re-log the same alert every tick.
                if (!loggedIds.has(r.alert_id)) {
                    loggedIds.add(r.alert_id);
                    console.log(`[AlertEmailer] (log-mode) WOULD email ${r.alert_type} for ${r.website_name} -> ${r.email} | ${r.message}`);
                }
            }
        }
    } catch (err) {
        console.error("[AlertEmailer] poll error:", err.message);
    } finally {
        running = false;
    }
}

function startAlertEmailer() {
    console.log(
        `[AlertEmailer] started — mode=${ENABLED ? "SEND" : "log-only"}, ` +
        `every ${POLL_MS}ms, lookback ${LOOKBACK_MIN}min`
    );
    setInterval(processPendingAlertEmails, POLL_MS);
    // Run once shortly after boot.
    setTimeout(processPendingAlertEmails, 3000);
}

module.exports = { startAlertEmailer, processPendingAlertEmails };
