#!/usr/bin/env bash
# ============================================================
# scripts/setup_cron.sh
# OS Cron Setup — Register Weekly Report Generator
# Owner: Rishita Ramola
#
# OS Concept Demonstrated:
#   - cron daemon (OS-level process scheduling)
#   - crontab -l / crontab - for non-destructive cron editing
#   - Environment variable forwarding to cron environment
#
# Usage:
#   chmod +x scripts/setup_cron.sh
#   ./scripts/setup_cron.sh
#
# What it does:
#   Registers the report_generator binary to run every
#   Sunday at 23:59 (11:59 PM) using the system cron daemon.
#   Loads environment from configs/.env so DB credentials
#   are available inside the cron job.
# ============================================================
set -euo pipefail

# ─── Configuration ────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARY="${PROJECT_ROOT}/build/report_generator"
ENV_FILE="${PROJECT_ROOT}/configs/.env"
LOG_FILE="${PROJECT_ROOT}/logs/report_generator_cron.log"
CRON_TAG="# webnotifier-report-generator"

# ─── Pre-flight checks ────────────────────────────────────────────────────────
echo "[setup_cron] Checking pre-requisites..."

if [ ! -f "${BINARY}" ]; then
    echo "[setup_cron] ERROR: Binary not found at ${BINARY}"
    echo "             Run: cmake -B build -S . && cmake --build build -j4"
    exit 1
fi

if [ ! -f "${ENV_FILE}" ]; then
    echo "[setup_cron] ERROR: ${ENV_FILE} not found."
    echo "             Run: cp configs/.env.example configs/.env"
    echo "             Then fill in your database credentials."
    exit 1
fi

# Create logs directory if it doesn't exist
mkdir -p "${PROJECT_ROOT}/logs"

# ─── Build the cron entry ─────────────────────────────────────────────────────
# Format: mm hh dom mon dow command
# 59 23 * * 0  → every Sunday at 23:59
# 'set -a; source .env; set +a' exports all .env vars into the cron shell.
CRON_CMD="59 23 * * 0  set -a && source \"${ENV_FILE}\" && set +a && \"${BINARY}\" >> \"${LOG_FILE}\" 2>&1 ${CRON_TAG}"

# ─── Install cron entry (idempotent — remove old entry first) ─────────────────
echo "[setup_cron] Removing any existing WebNotifier cron entry..."
# crontab -l lists current crontab; grep -v removes our tagged line;
# then we pipe back in. Handles the edge case of an empty crontab gracefully.
(crontab -l 2>/dev/null || true) \
    | grep -v "${CRON_TAG}" \
    | { cat; echo "${CRON_CMD}"; } \
    | crontab -

echo ""
echo "[setup_cron] ✓ Cron job registered successfully!"
echo ""
echo "  Schedule  : Every Sunday at 23:59"
echo "  Binary    : ${BINARY}"
echo "  Env file  : ${ENV_FILE}"
echo "  Log file  : ${LOG_FILE}"
echo ""
echo "  Verify with: crontab -l | grep webnotifier"
echo ""
