#!/usr/bin/env bash
# ============================================================
# scripts/start_scheduler.sh
# Launch the Scheduler Daemon as a background OS process
# Owner: Rishita Ramola
#
# OS Concepts Demonstrated:
#   - nohup   — detaches process from the terminal (SIGHUP immunity)
#   - &       — background process execution
#   - PID file — process management convention
#   - source  — load environment variables from .env
#
# Usage:
#   chmod +x scripts/start_scheduler.sh
#   ./scripts/start_scheduler.sh
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${PROJECT_ROOT}/build/scheduler_daemon"
ENV_FILE="${PROJECT_ROOT}/configs/.env"
PID_FILE="${PROJECT_ROOT}/logs/scheduler.pid"
LOG_FILE="${PROJECT_ROOT}/logs/scheduler.log"

# ─── Pre-flight checks ────────────────────────────────────────────────────────
if [ ! -f "${BINARY}" ]; then
    echo "[start_scheduler] ERROR: Binary not found: ${BINARY}"
    echo "  Build first: cmake -B build -S . && cmake --build build -j4"
    exit 1
fi

if [ ! -f "${ENV_FILE}" ]; then
    echo "[start_scheduler] ERROR: ${ENV_FILE} not found."
    echo "  Run: cp configs/.env.example configs/.env && nano configs/.env"
    exit 1
fi

# ─── Check if already running ─────────────────────────────────────────────────
if [ -f "${PID_FILE}" ]; then
    OLD_PID=$(cat "${PID_FILE}")
    if kill -0 "${OLD_PID}" 2>/dev/null; then
        echo "[start_scheduler] Scheduler is already running (PID=${OLD_PID})."
        echo "  Use scripts/stop_scheduler.sh to stop it first."
        exit 0
    else
        echo "[start_scheduler] Stale PID file found — cleaning up."
        rm -f "${PID_FILE}"
    fi
fi

# ─── Load environment variables ───────────────────────────────────────────────
# shellcheck source=../configs/.env
set -a
# shellcheck disable=SC1090
source "${ENV_FILE}"
set +a

# ─── Create logs directory ────────────────────────────────────────────────────
mkdir -p "${PROJECT_ROOT}/logs"

# ─── Launch daemon with nohup ─────────────────────────────────────────────────
echo "[start_scheduler] Starting scheduler_daemon..."
nohup "${BINARY}" >> "${LOG_FILE}" 2>&1 &
DAEMON_PID=$!

# Write PID file for stop_scheduler.sh to use
echo "${DAEMON_PID}" > "${PID_FILE}"

# Brief pause to confirm it didn't crash immediately
sleep 1
if kill -0 "${DAEMON_PID}" 2>/dev/null; then
    echo "[start_scheduler] ✓ Daemon started successfully."
    echo "  PID      : ${DAEMON_PID}"
    echo "  PID file : ${PID_FILE}"
    echo "  Log file : ${LOG_FILE}"
    echo ""
    echo "  Monitor:  tail -f ${LOG_FILE}"
    echo "  Stop:     ./scripts/stop_scheduler.sh"
else
    echo "[start_scheduler] ✗ Daemon exited immediately. Check log:"
    tail -n 20 "${LOG_FILE}"
    rm -f "${PID_FILE}"
    exit 1
fi
