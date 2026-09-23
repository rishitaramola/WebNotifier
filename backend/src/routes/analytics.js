const express = require("express");
const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");

// GET /api/analytics/summary
router.get("/summary", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                total_sites,
                sites_up,
                sites_down,
                sites_unknown,
                overall_uptime_pct,
                alerts_last_24h,
                alerts_last_7d
            FROM v_user_dashboard_summary
            WHERE user_id = $1;
            `,
            [req.session.userId]
        );

        if (result.rows.length === 0) {
            return res.json({
                total_sites: 0,
                sites_up: 0,
                sites_down: 0,
                sites_unknown: 0,
                overall_uptime_pct: 0,
                alerts_last_24h: 0,
                alerts_last_7d: 0
            });
        }

        res.json(result.rows[0]);
    } catch (error) {
        console.error("[Analytics] Summary error:", error.message);

        res.status(500).json({
            error: "Failed to fetch analytics summary"
        });
    }
});

// GET /api/analytics/uptime
router.get("/uptime", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                website_id,
                url,
                name,
                total_checks,
                up_count,
                downtime_incidents,
                uptime_pct,
                avg_response_ms,
                downtime_minutes,
                total_alerts
            FROM v_weekly_summary
            WHERE user_id = $1
            ORDER BY website_id;
            `,
            [req.session.userId]
        );

        res.json(result.rows);
    } catch (error) {
        console.error("[Analytics] Uptime error:", error.message);

        res.status(500).json({
            error: "Failed to fetch uptime statistics"
        });
    }
});

module.exports = router;
