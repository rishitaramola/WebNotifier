const express = require("express");
const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");

// GET /api/websites
router.get("/", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT *
            FROM v_dashboard_website_cards
            WHERE user_id = $1
            ORDER BY website_id;
            `,
            [req.session.userId]
        );

        res.json(result.rows);
    } catch (error) {
        console.error("[Websites] GET error:", error.message);

        res.status(500).json({
            error: "Failed to fetch websites"
        });
    }
});

// POST /api/websites
router.post("/", requireAuth, async (req, res) => {
    try {
        const {
            url,
            name,
            check_interval_min,
            keyword,
            timeout_seconds,
            notify_email
        } = req.body;

        if (!url || !name) {
            return res.status(400).json({
                error: "URL and name are required"
            });
        }

        const result = await pool.query(
            `
            INSERT INTO websites
                (
                    user_id,
                    url,
                    name,
                    check_interval_min,
                    keyword,
                    timeout_seconds,
                    notify_email
                )
            VALUES
                ($1, $2, $3, $4, $5, $6, $7)
            RETURNING *;
            `,
            [
                req.session.userId,
                url,
                name,
                parseInt(check_interval_min) || 5,
                keyword || null,
                parseInt(timeout_seconds) || 30,
                notify_email || null
            ]
        );

        res.status(201).json(result.rows[0]);
    } catch (error) {
        console.error("[Websites] POST error:", error.message);

        res.status(500).json({
            error: "Failed to add website"
        });
    }
});

// GET /api/websites/ssl-warnings
router.get("/ssl-warnings", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                website_id,
                url,
                name,
                alert_threshold_days,
                days_remaining,
                last_checked_at,
                urgency
            FROM v_ssl_expiry_warnings
            WHERE user_id = $1
            ORDER BY days_remaining ASC;
            `,
            [req.session.userId]
        );

        res.json(result.rows);
    } catch (error) {
        console.error("[Websites] SSL warnings error:", error.message);

        res.status(500).json({
            error: "Failed to fetch SSL warnings"
        });
    }
});

// DELETE /api/websites/:id
router.delete("/:id", requireAuth, async (req, res) => {
    try {
        const websiteId = parseInt(req.params.id);

        if (isNaN(websiteId)) {
            return res.status(400).json({
                error: "Invalid website ID"
            });
        }

        const result = await pool.query(
            `
            DELETE FROM websites
            WHERE id = $1
              AND user_id = $2
            RETURNING id;
            `,
            [websiteId, req.session.userId]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({
                error: "Website not found"
            });
        }

        res.json({
            message: "Website deleted successfully",
            website_id: result.rows[0].id
        });
    } catch (error) {
        console.error("[Websites] DELETE error:", error.message);

        res.status(500).json({
            error: "Failed to delete website"
        });
    }
});

module.exports = router;
