const express = require("express");

const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");

// GET /api/system-metrics
router.get("/", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                id,
                cpu_usage_pct,
                mem_usage_pct,
                active_threads,
                queue_depth,
                recorded_at,
                cpu_status,
                mem_status
            FROM v_system_health_latest
            LIMIT 1;
            `
        );

        if (result.rows.length === 0) {
            return res.json({
                cpu_usage_pct: 0,
                mem_usage_pct: 0,
                active_threads: 0,
                queue_depth: 0,
                recorded_at: null,
                cpu_status: "healthy",
                mem_status: "healthy"
            });
        }

        res.json(result.rows[0]);
    } catch (error) {
        console.error("[System Metrics] GET error:", error.message);

        res.status(500).json({
            error: "Failed to fetch system metrics"
        });
    }
});

// GET /api/system-metrics/history
router.get("/history", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `
            SELECT
                id,
                cpu_usage_pct,
                mem_usage_pct,
                active_threads,
                queue_depth,
                recorded_at
            FROM v_system_metrics_history
            ORDER BY recorded_at ASC;
            `
        );

        res.json(result.rows);
    } catch (error) {
        console.error(
            "[System Metrics] History error:",
            error.message
        );

        res.status(500).json({
            error: "Failed to fetch system metrics history"
        });
    }
});

module.exports = router;
