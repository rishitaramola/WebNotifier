const express = require("express");

const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");

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
                acknowledged,
                badge_class
            FROM v_recent_alerts
            WHERE user_id = $1
            ORDER BY sent_at DESC
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

module.exports = router;
