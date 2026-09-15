#!/usr/bin/env bash
# ============================================================
# scripts/stop_scheduler.sh
# Gracefully stop the Scheduler Daemon
# Owner: Rishita Ramola
#
# OS Concepts Demonstrated:
#   - kill -TERM (SIGTERM) — request graceful shutdown
#   - kill -KILL (SIGKILL) — force-kill after timeout
#   - PID file management
#   - Process existence check with kill -0
#
# Usage:
#   ./scripts/stop_scheduler.sh
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
PID_FILE="${PROJECT_ROOT}/logs/scheduler.pid"
TIMEOUT=15   # seconds to wait for graceful shutdown

# ─── Read PID ─────────────────────────────────────────────────────────────────
if [ ! -f "${PID_FILE}" ]; then
    echo "[stop_scheduler] No PID file found at ${PID_FILE}."
    echo "  The scheduler may not be running."
    exit 0
fi

PID=$(cat "${PID_FILE}")

# Validate PID is a number
if ! [[ "${PID}" =~ ^[0-9]+$ ]]; then
    echo "[stop_scheduler] Invalid PID '${PID}' in PID file."
    rm -f "${PID_FILE}"
    exit 1
fi

# ─── Check if process exists ──────────────────────────────────────────────────
if ! kill -0 "${PID}" 2>/dev/null; then
    echo "[stop_scheduler] Process ${PID} is not running (stale PID file)."
    rm -f "${PID_FILE}"
    exit 0
fi

# ─── Send SIGTERM (graceful shutdown) ─────────────────────────────────────────
echo "[stop_scheduler] Sending SIGTERM to PID ${PID}..."
kill -TERM "${PID}"

# ─── Wait for graceful exit ───────────────────────────────────────────────────
for i in $(seq 1 "${TIMEOUT}"); do
    if ! kill -0 "${PID}" 2>/dev/null; then
        echo "[stop_scheduler] ✓ Scheduler stopped gracefully (${i}s)."
        rm -f "${PID_FILE}"
        exit 0
    fi
    sleep 1
done

# ─── Force-kill if still alive after TIMEOUT ──────────────────────────────────
echo "[stop_scheduler] Daemon did not stop in ${TIMEOUT}s — sending SIGKILL..."
kill -KILL "${PID}" 2>/dev/null || true
sleep 1
rm -f "${PID_FILE}"
echo "[stop_scheduler] ✓ Daemon force-killed."
