# Running WebNotifier

This project has **three runnable parts**. They run as separate processes.

| Part | Tech | Port | Role |
|------|------|------|------|
| Backend API | Node.js / Express | 8080 | REST API + auth, reads/writes PostgreSQL |
| Frontend | Static HTML/CSS/JS | 5500 | Dashboard (served by any static server) |
| Scheduler + Worker Pool | C++ (`scheduler_daemon`) | — | Checks websites every 60s, writes `monitoring_results` |

Database: **PostgreSQL** named `webnotifier` (default port 5433 per `backend/.env`).

---

## Prerequisites

```bash
# Node + static server + C++ build toolchain
node --version          # v18+
psql --version          # PostgreSQL client
cmake --version         # 3.16+
# C++ libs: libpq-dev, libcurl, openssl   (already present if the daemon builds)
```

---

## One-time setup (fresh machine only)

```bash
# 1. Create database + load schema/seed (run from the WebNotifier/ folder)
export PGPASSWORD='<your_db_password>'
createdb -h localhost -p 5433 -U postgres webnotifier
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/schema/001_create_tables.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/schema/002_create_indexes.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/views/001_create_views.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/views/002_dashboard_views.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/migrations/003_add_ssl_threshold.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/migrations/004_analytics_views.sql
psql -h localhost -p 5433 -U postgres -d webnotifier -f database/seeds/seed_data.sql

# 2. Create env files from templates and fill in real values
cp backend/.env.example backend/.env     # Node reads this
cp configs/.env.example configs/.env     # C++ scheduler/report scripts read this

# 3. Install Node dependencies
cd backend && npm install && cd ..
```

---

## Run (every time)

Open **three terminals** from the `WebNotifier/` folder.

### 1. Backend API (port 8080)
```bash
cd backend
node src/server.js
# -> [Server] WebNotifier API running on port 8080
```

### 2. Frontend (port 5500 — must be 5500, backend CORS only allows 5500/3000)
```bash
cd frontend
python3 -m http.server 5500
```

### 3. Scheduler + Worker Pool (the actual website checker)
```bash
# Build once:
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target scheduler_daemon -j4

# Run (needs DB env vars):
export DB_HOST=localhost DB_PORT=5433 DB_NAME=webnotifier DB_USER=postgres DB_PASSWORD='<your_db_password>'
export WORKER_COUNT=4 QUEUE_CAPACITY=1000
./build/scheduler_daemon
# -> [Scheduler] Loaded N website(s) from DB.  ... checks every 60s
```

Then open **http://localhost:5500** in a browser.

---

## Stop

```bash
pkill -f "node src/server.js"
pkill -f "http.server 5500"
pkill -f scheduler_daemon
```

---

## Verify it's working

```bash
curl http://localhost:8080/api/health           # {"status":"ok",...}
# Fresh checks appearing in the DB:
psql -h localhost -p 5433 -U postgres -d webnotifier \
  -c "SELECT count(*) FROM monitoring_results WHERE checked_at > NOW() - INTERVAL '2 minutes';"
```

---

## Notes / gotchas

- **Frontend port must be 5500** — `backend/src/server.js` CORS whitelist allows only
  `localhost:5500`, `127.0.0.1:5500`, `localhost:3000`. Any other port blocks login.
- **Two `.env` files**: `backend/.env` (Node) and `configs/.env` (C++ scripts). Keep
  both in sync for DB credentials. Neither is committed (see `.gitignore`).
- **Sites show up/down only after the scheduler runs** — the dashboard reads
  `monitoring_results`, which only the C++ `scheduler_daemon` writes.
- **Known bug**: down-alert inserts fail with a `text vs varchar` type mismatch in
  `worker_pool/src/db_writer.cpp` (monitoring works; alert rows aren't saved). Fix is a
  `::varchar` cast on the parameterized alert insert.
