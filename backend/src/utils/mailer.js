const nodemailer = require("nodemailer");

// Create a reusable transporter using SMTP settings from .env
// Works with Gmail (App Password), Outlook, or any SMTP provider.
const transporter = nodemailer.createTransport({
    host: process.env.SMTP_HOST || "smtp.gmail.com",
    port: parseInt(process.env.SMTP_PORT || "587"),
    secure: process.env.SMTP_SECURE === "true", // true for port 465, false for 587
    auth: {
        user: process.env.SMTP_USER,
        pass: process.env.SMTP_PASS,
    },
});

/**
 * Send a password-reset email with a temporary password.
 * @param {string} toEmail   - recipient email address
 * @param {string} username  - recipient username (for personalisation)
 * @param {string} tempPass  - the generated temporary password
 */
async function sendPasswordResetEmail(toEmail, username, tempPass) {
    const mailOptions = {
        from: `"WebNotifier" <${process.env.SMTP_USER}>`,
        to: toEmail,
        subject: "WebNotifier – Password Reset",
        text: `Hi ${username},\n\nA password reset was requested for your account.\n\nYour temporary password is:\n\n    ${tempPass}\n\nPlease log in with this password and change it immediately from Settings > Change Password.\n\nIf you did not request this, you can ignore this email — your account is safe.\n\nThanks,\nWebNotifier Team`,
        html: `
<!DOCTYPE html>
<html>
<body style="font-family: Arial, sans-serif; background:#f4f4f4; padding:20px;">
  <div style="max-width:500px; margin:auto; background:#fff; border-radius:8px; padding:30px;">
    <h2 style="color:#333;">Password Reset</h2>
    <p>Hi <strong>${username}</strong>,</p>
    <p>A password reset was requested for your WebNotifier account.</p>
    <p>Your temporary password is:</p>
    <div style="background:#f0f0f0; padding:12px 20px; border-radius:6px; font-size:18px; letter-spacing:2px; font-family:monospace; text-align:center;">
      ${tempPass}
    </div>
    <p style="margin-top:20px;">Please log in with this password and change it immediately from <strong>Settings &rarr; Change Password</strong>.</p>
    <p style="color:#888; font-size:12px;">If you did not request this reset, you can safely ignore this email.</p>
    <hr style="border:none; border-top:1px solid #eee; margin:20px 0;">
    <p style="color:#aaa; font-size:11px;">WebNotifier &ndash; Automated Website Monitoring</p>
  </div>
</body>
</html>`,
    };

    await transporter.sendMail(mailOptions);
    console.log(`[Mailer] Password reset email sent to ${toEmail}`);
}

/**
 * Send an alert notification email when a website issue is detected.
 * @param {string} toEmail     - recipient email
 * @param {string} username    - recipient username
 * @param {object} alert       - alert details { websiteName, url, alertType, message }
 */
async function sendAlertEmail(toEmail, username, alert) {
    const { websiteName, url, alertType, message } = alert;

    // Choose a colour/icon based on alert severity
    const isDown = alertType && alertType.toUpperCase().includes("DOWN");
    const accentColor = isDown ? "#e74c3c" : "#e67e22";
    const statusLabel = isDown ? "🔴 DOWN" : "⚠️ WARNING";

    const mailOptions = {
        from: `"WebNotifier Alerts" <${process.env.SMTP_USER}>`,
        to: toEmail,
        subject: `WebNotifier Alert – ${websiteName} is ${isDown ? "DOWN" : "degraded"}`,
        text: `Hi ${username},\n\nAn alert was triggered for your monitored website:\n\nWebsite : ${websiteName}\nURL     : ${url}\nType    : ${alertType}\nDetails : ${message}\n\nLog in to your WebNotifier dashboard to view full details.\n\nWebNotifier Team`,
        html: `
<!DOCTYPE html>
<html>
<body style="font-family: Arial, sans-serif; background:#f4f4f4; padding:20px;">
  <div style="max-width:500px; margin:auto; background:#fff; border-radius:8px; padding:30px;">
    <h2 style="color:${accentColor};">${statusLabel} &ndash; ${websiteName}</h2>
    <p>Hi <strong>${username}</strong>,</p>
    <p>An alert was triggered for one of your monitored websites:</p>
    <table style="width:100%; border-collapse:collapse; margin:16px 0;">
      <tr>
        <td style="padding:8px; background:#f9f9f9; font-weight:bold; width:30%;">Website</td>
        <td style="padding:8px;">${websiteName}</td>
      </tr>
      <tr>
        <td style="padding:8px; background:#f9f9f9; font-weight:bold;">URL</td>
        <td style="padding:8px;"><a href="${url}" style="color:#3498db;">${url}</a></td>
      </tr>
      <tr>
        <td style="padding:8px; background:#f9f9f9; font-weight:bold;">Alert Type</td>
        <td style="padding:8px;">${alertType}</td>
      </tr>
      <tr>
        <td style="padding:8px; background:#f9f9f9; font-weight:bold;">Details</td>
        <td style="padding:8px;">${message}</td>
      </tr>
    </table>
    <a href="http://localhost:5500" style="display:inline-block; background:${accentColor}; color:#fff; padding:10px 20px; border-radius:6px; text-decoration:none;">View Dashboard</a>
    <hr style="border:none; border-top:1px solid #eee; margin:20px 0;">
    <p style="color:#aaa; font-size:11px;">WebNotifier &ndash; Automated Website Monitoring</p>
  </div>
</body>
</html>`,
    };

    await transporter.sendMail(mailOptions);
    console.log(`[Mailer] Alert email sent to ${toEmail} for ${websiteName}`);
}

module.exports = { sendPasswordResetEmail, sendAlertEmail };
