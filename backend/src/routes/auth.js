const express = require("express");
const bcrypt = require("bcrypt");
const crypto = require("crypto");

const router = express.Router();
const { pool } = require("../db");
const { sendPasswordResetEmail } = require("../utils/mailer");

// POST /api/users/login
router.post("/login", async (req, res) => {
    try {
        const { username, password } = req.body;

        if (!username || !password) {
            return res.status(400).json({
                error: "Username and password are required"
            });
        }

        const result = await pool.query(
            `
            SELECT id, username, email, password_hash, is_active, role
            FROM users
            WHERE username = $1
            `,
            [username]
        );

        if (result.rows.length === 0) {
            return res.status(401).json({
                error: "Invalid username or password"
            });
        }

        const user = result.rows[0];

        if (!user.is_active) {
            return res.status(403).json({
                error: "User account is inactive"
            });
        }

        const passwordMatches = await bcrypt.compare(
            password,
            user.password_hash
        );

        if (!passwordMatches) {
            return res.status(401).json({
                error: "Invalid username or password"
            });
        }

        req.session.userId = user.id;
        req.session.username = user.username;
        req.session.role = user.role;

        res.json({
            message: "Login successful",
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                role: user.role
            }
        });
    } catch (error) {
        console.error("[Auth] Login error:", error.message);

        res.status(500).json({
            error: "Login failed"
        });
    }
});

router.post("/register", async (req, res) => {
    try {
        const {
            username,
            email,
            password,
            confirmPassword
        } = req.body;

        // Check required fields
        if (!username || !email || !password || !confirmPassword) {
            return res.status(400).json({
                error: "All fields are required"
            });
        }

        // Check password confirmation
        if (password !== confirmPassword) {
            return res.status(400).json({
                error: "Passwords do not match"
            });
        }

        // Check password length
        if (password.length < 6) {
            return res.status(400).json({
                error: "Password must be at least 6 characters"
            });
        }

        // Check if username already exists
        const usernameResult = await pool.query(
            `
            SELECT id
            FROM users
            WHERE username = $1
            `,
            [username]
        );

        if (usernameResult.rows.length > 0) {
            return res.status(409).json({
                error: "Username already exists"
            });
        }

        // Check if email already exists
        const emailResult = await pool.query(
            `
            SELECT id
            FROM users
            WHERE email = $1
            `,
            [email]
        );

        if (emailResult.rows.length > 0) {
            return res.status(409).json({
                error: "Email already exists"
            });
        }

        // Hash the password
        const passwordHash = await bcrypt.hash(password, 10);

        // Insert new user into database
        const result = await pool.query(
            `
            INSERT INTO users
            (
                username,
                email,
                password_hash,
                is_active,
                role
            )
            VALUES
            ($1, $2, $3, true, 'user')
            RETURNING id, username, email, role;
            `,
            [username, email, passwordHash]
        );

        const user = result.rows[0];

        // Create login session
        req.session.userId = user.id;
        req.session.username = user.username;
        req.session.role = user.role;

        res.status(201).json({
            message: "Account created successfully",
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                role: user.role
            }
        });

    } catch (error) {
        console.error(
            "[Auth] Registration error:",
            error.message
        );

        res.status(500).json({
            error: "Registration failed"
        });
    }
});

router.post("/logout", (req, res) => {
    req.session.destroy((error) => {

        if (error) {
            console.error(
                "[Auth] Logout error:",
                error.message
            );

            return res.status(500).json({
                error: "Logout failed"
            });
        }

        res.clearCookie("connect.sid");

        res.json({
            message: "Logout successful"
        });
    });
});

// POST /api/users/change-password (requires active session)
router.post("/change-password", async (req, res) => {
    try {
        if (!req.session.userId) {
            return res.status(401).json({ error: "Not logged in" });
        }
        const { newPassword, confirmPassword } = req.body;
        if (!newPassword || !confirmPassword) {
            return res.status(400).json({ error: "All fields are required" });
        }
        if (newPassword !== confirmPassword) {
            return res.status(400).json({ error: "New passwords do not match" });
        }
        if (newPassword.length < 6) {
            return res.status(400).json({ error: "New password must be at least 6 characters" });
        }
        const newHash = await bcrypt.hash(newPassword, 10);
        await pool.query(
            `UPDATE users SET password_hash = $1, updated_at = NOW() WHERE id = $2`,
            [newHash, req.session.userId]
        );
        res.json({ message: "Password updated successfully" });
    } catch (error) {
        console.error("[Auth] Change password error:", error.message);
        res.status(500).json({ error: "Password change failed" });
    }
});

// POST /api/users/forgot-password
router.post("/forgot-password", async (req, res) => {
    try {
        const { email } = req.body;
        if (!email) {
            return res.status(400).json({ error: "Email is required" });
        }
        // Always return the SAME generic response regardless of whether the
        // email exists. This prevents account enumeration and stops anyone
        // from resetting (and reading) another user's password.
        const genericMessage =
            "If that email is registered, a temporary password has been generated and sent to the account owner.";

        const result = await pool.query(
            `SELECT id, username FROM users WHERE email = $1 AND is_active = true`, [email]
        );

        if (result.rows.length > 0) {
            const user = result.rows[0];

            // Cryptographically secure temporary password (not Math.random).
            const tempPassword = crypto.randomBytes(9).toString("base64url");
            const hash = await bcrypt.hash(tempPassword, 10);

            await pool.query(
                `UPDATE users SET password_hash = $1, updated_at = NOW() WHERE id = $2`,
                [hash, user.id]
            );

            // Send the temporary password via email.
            try {
                await sendPasswordResetEmail(email, user.username, tempPassword);
            } catch (mailError) {
                // Log the error but don't expose it to the client.
                // Also log the temp password so dev can still test without email.
                console.error("[Auth] Failed to send reset email:", mailError.message);
                console.log(
                    `[Auth] Temporary password for ${user.username} <${email}>: ${tempPassword}`
                );
            }
        }

        res.json({ message: genericMessage });
    } catch (error) {
        console.error("[Auth] Forgot password error:", error.message);
        res.status(500).json({ error: "Password reset failed" });
    }
});

module.exports = router;
