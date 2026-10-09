const express = require("express");
const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");
const { normalizeUrl } = require("../utils/urlValidator");

// POST /api/websites/check-now
// Signals the C++ scheduler to immediately enqueue all active sites.
// Debounced: ignores a request if one was made in the last CHECK_NOW_DEBOUNCE_SEC
// seconds, so rapid dashboard refreshes don't flood the queue / monitored sites.
const CHECK_NOW_DEBOUNCE_SEC = parseInt(process.env.CHECK_NOW_DEBOUNCE_SEC || "30", 10);
router.post("/check-now", requireAuth, async (req, res) => {
    try {
        // `force` (sent right after adding a website) bypasses the debounce so a
        // brand-new site is always checked immediately.
        const force = req.body && req.body.force === true;
        if (!force) {
            const recent = await pool.query(
                `SELECT 1 FROM check_requests
                 WHERE requested_at >= NOW() - ($1 || ' seconds')::interval
                 LIMIT 1`,
                [String(CHECK_NOW_DEBOUNCE_SEC)]
            );
            if (recent.rows.length > 0) {
                return res.json({ queued: false, debounced: true,
                    message: `A check was already requested in the last ${CHECK_NOW_DEBOUNCE_SEC}s` });
            }
        }
        await pool.query(
            "INSERT INTO check_requests (requested_by) VALUES ($1)",
            [req.session.userId]
        );
        res.json({ queued: true, message: "Immediate check requested" });
    } catch (error) {
        console.error("[Websites] check-now error:", error.message);
        res.status(500).json({ error: "Failed to request check" });
    }
});

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

        // Normalize + validate the URL authoritatively (fixes "https:hotstar.com",
        // bare domains, etc.; rejects non-http(s) junk).
        const cleanUrl = normalizeUrl(url);
        if (!cleanUrl) {
            return res.status(400).json({
                error: "Please enter a valid website URL (e.g. https://example.com)"
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
                cleanUrl,
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
