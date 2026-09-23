const systemMetricsRouter = require("./routes/systemMetrics");
const alertsRouter = require("./routes/alerts");
const express = require("express");
const cors = require("cors");
const session = require("express-session");
require("dotenv").config();

const { testConnection } = require("./db");
const websitesRouter = require("./routes/websites");
const authRouter = require("./routes/auth");
const analyticsRouter = require("./routes/analytics");

const app = express();

app.use(
    cors({
        origin: "http://localhost:5500",
        credentials: true
    })
);

app.use(express.json());

app.use(
    session({
        secret: process.env.SESSION_SECRET,
        resave: false,
        saveUninitialized: false,
        cookie: {
            httpOnly: true,
            maxAge: 1000 * 60 * 60 * 24
        }
    })
);

app.use("/api/users", authRouter);
app.use("/api/websites", websitesRouter);
app.use("/api/analytics", analyticsRouter);
app.use("/api/alerts", alertsRouter);
app.use("/api/system-metrics", systemMetricsRouter);

app.get("/api/health", (req, res) => {
    res.json({
        status: "ok",
        message: "WebNotifier backend is running"
    });
});

const PORT = process.env.PORT || 8080;

async function startServer() {
    const dbConnected = await testConnection();

    if (!dbConnected) {
        console.error("[Server] Could not connect to database.");
        process.exit(1);
    }

    app.listen(PORT, () => {
        console.log(`[Server] WebNotifier API running on port ${PORT}`);
    });
}

startServer();
