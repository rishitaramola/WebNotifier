#!/usr/bin/env bash
# ============================================================
# start_all.sh — launch all three WebNotifier services
# Backend API (8080) + Frontend (5500) + Scheduler/Worker daemon
# Run from anywhere:  ./scripts/start_all.sh
# ============================================================
set -euo pipefail

# Resolve project root (parent of this scripts/ folder)
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p logs

# ── Load DB credentials for the C++ daemon from backend/.env ──
# (Node reads backend/.env itself via dotenv; the C++ daemon needs them exported.)
if [[ -f backend/.env ]]; then
    set -a
    # shellcheck disable=SC1091
    source backend/.env
    set +a
fi

echo "[start_all] Project: $ROOT"

# ── 1. Backend API ───────────────────────────────────────────
if pgrep -f "node src/server.js" >/dev/null; then
    echo "[start_all] Backend already running."
else
    ( cd backend && nohup node src/server.js >"$ROOT/logs/backend.log" 2>&1 & )
    echo "[start_all] Backend API started on :${PORT:-8080}  (logs/backend.log)"
fi

# ── 2. Frontend (static) ─────────────────────────────────────
if pgrep -f "http.server 5500" >/dev/null; then
    echo "[start_all] Frontend already running."
else
    ( cd frontend && nohup python3 -m http.server 5500 --bind 127.0.0.1 \
        >"$ROOT/logs/frontend.log" 2>&1 & )
    echo "[start_all] Frontend started on http://localhost:5500  (logs/frontend.log)"
fi

# ── 3. Scheduler + Worker Pool daemon ────────────────────────
if [[ ! -x build/scheduler_daemon ]]; then
    echo "[start_all] Building scheduler_daemon (first run)..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build build --target scheduler_daemon -j4 >/dev/null
fi

if pgrep -f "scheduler_daemon" >/dev/null; then
    echo "[start_all] Scheduler already running."
else
    nohup ./build/scheduler_daemon >"$ROOT/logs/scheduler_daemon.log" 2>&1 &
    echo "[start_all] Scheduler+Worker started  (logs/scheduler_daemon.log)"
fi

# ── 4. System Monitor daemon (host CPU/memory metrics) ───────
if [[ ! -x build/system_monitor_daemon ]]; then
    cmake --build build --target system_monitor_daemon -j4 >/dev/null 2>&1 || true
fi
if pgrep -f "system_monitor_daemon" >/dev/null; then
    echo "[start_all] System monitor already running."
elif [[ -x build/system_monitor_daemon ]]; then
    MONITOR_INTERVAL_SEC="${MONITOR_INTERVAL_SEC:-30}" \
        nohup ./build/system_monitor_daemon >"$ROOT/logs/system_monitor.log" 2>&1 &
    echo "[start_all] System monitor started  (logs/system_monitor.log)"
fi

echo
echo "[start_all] All services launched. Open http://localhost:5500"
echo "[start_all] Stop everything with: ./scripts/stop_all.sh"
