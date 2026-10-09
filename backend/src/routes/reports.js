const express = require("express");
const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");

const router = express.Router();

const { pool } = require("../db");
const requireAuth = require("../middleware/auth");

// Project root (…/backend/src/routes -> …/)
const PROJECT_ROOT = path.resolve(__dirname, "../../..");
const REPORTS_DIR = path.join(PROJECT_ROOT, "reports");
const REPORT_BIN = path.join(PROJECT_ROOT, "build", "report_generator");

// GET /api/reports — list this user's weekly reports (newest first)
router.get("/", requireAuth, async (req, res) => {
    try {
        const result = await pool.query(
            `SELECT id, week_start, week_end, total_checks, uptime_pct,
                    downtime_min, avg_response_ms, total_alerts,
                    report_path, generated_at
             FROM weekly_reports
             WHERE user_id = $1
             ORDER BY generated_at DESC`,
            [req.session.userId]
        );
        // Flag whether the CSV file still exists on disk (downloadable).
        const rows = result.rows.map((r) => ({
            ...r,
            downloadable: !!r.report_path && fs.existsSync(r.report_path),
        }));
        res.json(rows);
    } catch (error) {
        console.error("[Reports] GET error:", error.message);
        res.status(500).json({ error: "Failed to fetch reports" });
    }
});

// GET /api/reports/:id/download — stream the CSV for one report
router.get("/:id/download", requireAuth, async (req, res) => {
    try {
        const { rows } = await pool.query(
            "SELECT report_path FROM weekly_reports WHERE id = $1 AND user_id = $2",
            [parseInt(req.params.id, 10), req.session.userId]
        );
        if (rows.length === 0) {
            return res.status(404).json({ error: "Report not found" });
        }
        const filePath = rows[0].report_path;
        // Path-safety: the file must live inside REPORTS_DIR (no traversal).
        const resolved = path.resolve(filePath || "");
        if (!resolved.startsWith(REPORTS_DIR + path.sep) || !fs.existsSync(resolved)) {
            return res.status(404).json({ error: "Report file is no longer available" });
        }
        res.download(resolved, path.basename(resolved));
    } catch (error) {
        console.error("[Reports] download error:", error.message);
        res.status(500).json({ error: "Failed to download report" });
    }
});

// POST /api/reports/generate — run the C++ report_generator on demand
router.post("/generate", requireAuth, async (req, res) => {
    if (!fs.existsSync(REPORT_BIN)) {
        return res.status(503).json({
            error: "Report generator is not built. Run: cmake --build build --target report_generator",
        });
    }
    const child = spawn(REPORT_BIN, [], {
        cwd: PROJECT_ROOT,
        env: { ...process.env, REPORTS_DIR },
    });

    let stderr = "";
    child.stderr.on("data", (d) => (stderr += d.toString()));
    child.on("error", (err) => {
        console.error("[Reports] generate spawn error:", err.message);
        if (!res.headersSent) res.status(500).json({ error: "Failed to start report generator" });
    });
    child.on("close", (code) => {
        // The generator exits non-zero only when the optional email dispatch is
        // skipped; the CSV + DB rows are still written, so treat it as success.
        console.log(`[Reports] report_generator exited code=${code}`);
        if (!res.headersSent) res.json({ message: "Reports generated", exitCode: code });
    });
});

module.exports = router;
