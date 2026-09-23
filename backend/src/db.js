const { Pool } = require("pg");
require("dotenv").config();

const pool = new Pool({
    host: process.env.DB_HOST || "localhost",
    port: process.env.DB_PORT || 5432,
    database: process.env.DB_NAME || "webnotifier",
    user: process.env.DB_USER || "postgres",
    password: process.env.DB_PASSWORD,
});

pool.on("error", (err) => {
    console.error("[Database] Unexpected PostgreSQL error:", err.message);
});

async function testConnection() {
    try {
        const result = await pool.query("SELECT NOW()");
        console.log("[Database] Connected to PostgreSQL");
        console.log("[Database] Server time:", result.rows[0].now);
        return true;
    } catch (error) {
        console.error("[Database] Connection failed:", error.message);
        return false;
    }
}

module.exports = {
    pool,
    testConnection,
};
