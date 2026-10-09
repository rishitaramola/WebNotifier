const express = require("express");

const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");
const { sendAlertEmail } = require("../utils/mailer");

// GET /api/alerts
router.get("/", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                alert_id,
                website_id,
                url,
                website_name,
                alert_type,
                message,
                sent_to,
                sent_at,
                created_at,
                acknowledged,
                badge_class
            FROM v_recent_alerts
            WHERE user_id = $1
            ORDER BY created_at DESC
            LIMIT 50;
            `,
            [req.session.userId]
        );

        res.json(result.rows);
    } catch (error) {
        console.error("[Alerts] GET error:", error.message);

        res.status(500).json({
            error: "Failed to fetch alerts"
        });
    }
});

// POST /api/alerts/send-email
// Called internally (by the C++ scheduler/worker via HTTP) when a check fails.
// Body: { websiteId, websiteName, url, alertType, message }
router.post("/send-email", requireAuth, async (req, res) => {
    try {
        const { websiteId, websiteName, url, alertType, message } = req.body;

        if (!websiteId || !websiteName || !url || !alertType || !message) {
            return res.status(400).json({ error: "Missing required alert fields" });
        }

        // Get the email address of the website owner
        const userResult = await pool.query(
            `SELECT u.email, u.username
             FROM users u
             JOIN websites w ON w.user_id = u.id
             WHERE w.id = $1 AND u.id = $2`,
            [websiteId, req.session.userId]
        );

        if (userResult.rows.length === 0) {
            return res.status(404).json({ error: "Website not found or access denied" });
        }

        const { email, username } = userResult.rows[0];

        await sendAlertEmail(email, username, { websiteName, url, alertType, message });

        res.json({ message: "Alert email sent successfully" });
    } catch (error) {
        console.error("[Alerts] send-email error:", error.message);
        res.status(500).json({ error: "Failed to send alert email" });
    }
});

// POST /api/alerts/notify-all
// Internal endpoint: scans recent unacknowledged alerts and emails the owners.
// Designed to be called by the scheduler (e.g. via a cron job / C++ HTTP call).
// No session required – protected by a shared secret header instead.
router.post("/notify-all", async (req, res) => {
    const secret = req.headers["x-notify-secret"];
    if (!secret || secret !== process.env.NOTIFY_SECRET) {
        return res.status(403).json({ error: "Forbidden" });
    }

    try {
        // Fetch all unacknowledged alerts from the last hour with owner emails
        const result = await pool.query(`
            SELECT
                a.id          AS alert_id,
                a.website_id,
                a.alert_type,
                a.message,
                a.sent_at,
                w.name        AS website_name,
                w.url,
                u.email,
                u.username
            FROM alerts a
            JOIN websites w ON w.id = a.website_id
            JOIN users   u ON u.id = w.user_id
            WHERE a.acknowledged = false
              AND a.sent_at >= NOW() - INTERVAL '1 hour'
            ORDER BY a.sent_at DESC
        `);

        let sent = 0;
        for (const row of result.rows) {
            try {
                await sendAlertEmail(row.email, row.username, {
                    websiteName: row.website_name,
                    url: row.url,
                    alertType: row.alert_type,
                    message: row.message,
                });
                sent++;
            } catch (mailError) {
                console.error(
                    `[Alerts] Failed to email ${row.email} for alert ${row.alert_id}:`,
                    mailError.message
                );
            }
        }

        res.json({ message: `Notified ${sent} alert(s) out of ${result.rows.length}` });
    } catch (error) {
        console.error("[Alerts] notify-all error:", error.message);
        res.status(500).json({ error: "Failed to process notifications" });
    }
});

module.exports = router;
