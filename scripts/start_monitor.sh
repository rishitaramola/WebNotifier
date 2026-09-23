#!/usr/bin/env bash
# ============================================================
# scripts/start_monitor.sh
# Launch the System Monitor Daemon as a background OS process
# Owner: Simran Negi
#
# OS Concepts Demonstrated:
#   - nohup   : detaches from terminal (SIGHUP immunity)
#   - &       : background process execution
#   - PID file: process management convention
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${PROJECT_ROOT}/build/system_monitor_daemon"
ENV_FILE="${PROJECT_ROOT}/configs/.env"
PID_FILE="${PROJECT_ROOT}/logs/system_monitor.pid"
LOG_FILE="${PROJECT_ROOT}/logs/system_monitor.log"

if [ ! -f "${BINARY}" ]; then
    echo "[start_monitor] ERROR: Binary not found: ${BINARY}"
    echo "  Build: cmake -B build -S . && cmake --build build -j4"
    exit 1
fi

if [ -f "${PID_FILE}" ]; then
    OLD_PID=$(cat "${PID_FILE}")
    if kill -0 "${OLD_PID}" 2>/dev/null; then
        echo "[start_monitor] Already running (PID=${OLD_PID})."
        exit 0
    fi
    rm -f "${PID_FILE}"
fi

set -a && source "${ENV_FILE}" && set +a
mkdir -p "${PROJECT_ROOT}/logs"

nohup "${BINARY}" >> "${LOG_FILE}" 2>&1 &
DAEMON_PID=$!
echo "${DAEMON_PID}" > "${PID_FILE}"

sleep 1
if kill -0 "${DAEMON_PID}" 2>/dev/null; then
    echo "[start_monitor] ✓ system_monitor_daemon started (PID=${DAEMON_PID})"
    echo "  Log: tail -f ${LOG_FILE}"
else
    echo "[start_monitor] ✗ Crashed immediately. Check: ${LOG_FILE}"
    rm -f "${PID_FILE}"
    exit 1
fi
