#!/usr/bin/env bash
# ============================================================
# stop_all.sh — stop all three WebNotifier services
# ============================================================
set -uo pipefail

stop() {
    local pat="$1" name="$2"
    if pgrep -f "$pat" >/dev/null; then
        pkill -f "$pat" && echo "[stop_all] Stopped $name"
    else
        echo "[stop_all] $name not running"
    fi
}

stop "system_monitor_daemon" "System Monitor"
stop "scheduler_daemon"     "Scheduler+Worker"
stop "http.server 5500"     "Frontend"
stop "node src/server.js"   "Backend API"

echo "[stop_all] Done."
