// ============================================================
// utils/urlValidator.js
// Normalize and validate a website URL before it is stored.
//
// Fixes malformed input (e.g. "https:hotstar.com" -> "https://hotstar.com",
// bare "hotstar.com" -> "https://hotstar.com") and rejects anything that is
// not a real http/https URL (e.g. "javascript:...", empty, no host).
// Returns the clean URL string, or null if it cannot be made valid.
// ============================================================
function normalizeUrl(raw) {
    let s = String(raw || "").trim();
    if (!s) return null;

    // Pull off an explicit http/https scheme (tolerating missing/extra slashes),
    // otherwise default to https.
    let scheme = "https";
    const m = s.match(/^(https?):[/]*/i);
    if (m) {
        scheme = m[1].toLowerCase();
        s = s.slice(m[0].length);
    }
    s = s.replace(/^[/]+/, ""); // strip any stray leading slashes
    if (!s) return null;

    let u;
    try {
        u = new URL(scheme + "://" + s);
    } catch {
        return null;
    }

    // Require a real host (must contain a dot, or be localhost).
    if (!u.hostname || (!u.hostname.includes(".") && u.hostname !== "localhost")) {
        return null;
    }

    // Canonical form; drop the bare trailing slash so it matches existing data.
    let out = u.href;
    if (u.pathname === "/" && !u.search && !u.hash) out = u.origin;
    return out;
}

module.exports = { normalizeUrl };
