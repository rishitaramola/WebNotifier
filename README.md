# WebNotifier - Automated Website Monitoring Platform

> Team CodeSync | University PBL Project | OS + DBMS Combined Domain

---

## Project Overview

WebNotifier is an automated website monitoring and observability platform.
It continuously monitors websites and web assets without human intervention,
records health metrics, dispatches email alerts on failures, and generates
weekly analytical reports.

---

## Team Members

| Member           | Role       | Module Ownership                   |
|------------------|------------|------------------------------------|
| Rishita Ramola   | Team Lead  | Task Scheduler, Analytics Engine   |
| Shivank Garg     | Backend    | Thread-Safe Queue, Worker Pool     |
| Divyansh Sood    | Backend    | Backend API Layer, Alert Dispatcher|
| Simran Negi      | Frontend   | Web Dashboard (HTML/CSS/JS)        |

---

## Architecture Overview

  Web Dashboard (Simran) [HTML/CSS/JS]
          |
  Backend API Layer (Divyansh) [C++ REST]
          |
  Scheduler (Rishita) --> Queue (Shivank) --> Worker Pool (Shivank)
                                                    |
                                         PostgreSQL Database
                                                    |
                                     Analytics Engine (Rishita)

---

## Repository Structure

  WebNotifier/
  +-- backend/         # C++ REST API Server (Divyansh)
  +-- scheduler/       # Task Scheduler cron-like (Rishita)
  +-- worker_pool/     # Thread Pool + HTTP Checks (Shivank)
  +-- queue/           # Thread-Safe Task Queue (Shivank)
  +-- analytics/       # Reports + Statistics (Rishita)
  +-- alert_service/   # SMTP Email Alerts (Divyansh)
  +-- database/        # PostgreSQL Schema + Seeds
  +-- frontend/        # Web Dashboard (Simran)
  +-- docs/            # Architecture + API Docs
  +-- tests/           # Unit + Integration Tests
  +-- scripts/         # Build + Run scripts
  +-- configs/         # Config files
  +-- reports/         # Generated reports output
  +-- samples/         # Sample data + JSON examples
  +-- README.md

---

## Quick Start

  1. Clone: git clone https://github.com/rishitaramola/WebNotifier.git
  2. Config: cp configs/.env.example configs/.env
  3. DB:     psql -U postgres -f database/schema/001_create_tables.sql
  4. Build:  ./scripts/build.sh
  5. Run:    ./scripts/run_backend.sh

---

## OS Concepts Demonstrated

  Multithreading      -> worker_pool/
  Mutexes             -> queue/
  Condition Variables -> queue/
  Scheduling          -> scheduler/
  Daemon Services     -> backend/
  Resource Monitoring -> backend/system_monitor.cpp
  Socket Programming  -> worker_pool/network_checker.cpp

## DBMS Concepts Demonstrated

  ER Modeling         -> database/schema/er_diagram.md
  Normalization       -> database/schema/ (3NF)
  Primary/Foreign Keys-> All CREATE TABLE statements
  Transactions        -> worker_pool/db_writer.cpp
  Views               -> database/views/
  Aggregation         -> analytics/
  Indexing            -> database/schema/002_create_indexes.sql

---

## Development Roadmap

- **Week 1:** DB Schema, API Contracts, Project Setup
- **Week 2:** Queue + Scheduler Implementation
- **Week 3:** Worker Pool + HTTP Checker
- **Week 4:** Frontend Dashboard
- **Week 5:** Analytics Engine + Reports
- **Week 6:** Integration + Testing
- **Week 7:** Alert Dispatcher + SMTP
- **Week 8:** Final Integration + Final Testing
- **Week 9:** Final Demo + Documentation

---

MIT License - Team CodeSync

---

## C++ Scheduler & Analytics Module

**Owner: Rishita Ramola (Team Lead)**

This section documents the Task Scheduler daemon and Analytics Engine,
which form the brain of WebNotifier's automated monitoring pipeline.

---

### Architecture

```
PostgreSQL (websites table)
         |
         | load_from_db() — every 60s tick
         v
   [ Scheduler Daemon ]   ← scheduler/src/scheduler_main.cpp
         |
         | Task { id, website_id, url, keyword, timeout_sec }
         v
   [ TaskQueue (Shivank) ] — thread-safe blocking FIFO
         |
         | (consumed by Worker Pool)
         v
   [ monitoring_results table ]
         |
         | SQL aggregation queries
         v
   [ Analytics Engine ]   ← analytics/src/analytics_engine.cpp
         |
    ┌────┴────────────────┐
    v                     v
  CSV report            PDF report (wkhtmltopdf)
    |                     |
    └──────────┬──────────┘
               v
   POST /api/internal/send-report  (Divyansh's email API)
```

---

### Module Files

| Path | Description |
|---|---|
| `scheduler/include/scheduler.h` | Scheduler class — daemon thread, mutex, backoff |
| `scheduler/include/scheduling_strategy.h` | Strategy pattern interface (Fixed / Adaptive) |
| `scheduler/src/scheduler.cpp` | Full implementation: libpq queries, exponential backoff |
| `scheduler/src/scheduler_main.cpp` | Daemon entry point with signal handling |
| `scheduler/src/report_generator_main.cpp` | Weekly report generator entry point |
| `analytics/include/analytics_engine.h` | AnalyticsEngine DAO interface |
| `analytics/src/analytics_engine.cpp` | SQL aggregations, PDF via wkhtmltopdf, libcurl email |
| `database/migrations/004_analytics_views.sql` | Analytical SQL views (read-only) |
| `tests/scheduler/test_scheduler.cpp` | 16 unit tests (no DB required) |
| `scripts/start_scheduler.sh` | Launch daemon via nohup |
| `scripts/stop_scheduler.sh` | Graceful SIGTERM → SIGKILL stop |
| `scripts/setup_cron.sh` | Register weekly cron job (Sunday 23:59) |
| `configs/.env.example` | Environment variable template |

---

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt-get install -y \
    cmake build-essential \
    libpq-dev \           # PostgreSQL C library (libpq)
    libcurl4-openssl-dev \ # HTTP client for email dispatch
    wkhtmltopdf            # PDF generation (optional — falls back to CSV)

# macOS (Homebrew)
brew install cmake postgresql libcurl wkhtmltopdf
```

---

### Configuration

```bash
# 1. Copy the template
cp configs/.env.example configs/.env

# 2. Fill in your values (NEVER commit this file)
nano configs/.env
#   DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD
#   INTERNAL_API_HOST, INTERNAL_API_KEY
#   WKHTMLTOPDF_PATH (optional)
#   REPORTS_DIR
```

---

### Build

```bash
# Configure (Release build)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Compile all targets
cmake --build build -j4

# Outputs:
#   build/scheduler_daemon    — task scheduler daemon
#   build/report_generator    — weekly PDF report generator
#   build/test_scheduler      — unit test runner
```

---

### Run the Scheduler Daemon

```bash
# Start as background daemon (nohup, PID file written)
./scripts/start_scheduler.sh

# Monitor live output
tail -f logs/scheduler.log

# Stop gracefully (SIGTERM → waits 15s → SIGKILL)
./scripts/stop_scheduler.sh
```

The daemon:
1. Connects to PostgreSQL (with exponential backoff: 1s → 2s → 4s … 64s).
2. Loads all active websites from the `websites` table.
3. Every 60 seconds, checks which sites are due using `FixedIntervalStrategy`
   (or `AdaptiveStrategy` if `SCHEDULER_STRATEGY=adaptive`).
4. Inserts a row into `monitoring_jobs` and pushes a `Task` into Shivank's queue.
5. Paces pushes 50ms apart to avoid queue flooding.
6. Hot-reloads website configs every 10 ticks (≈10 minutes).

---

### Run the Report Generator Manually

```bash
# Load .env and run
set -a && source configs/.env && set +a
./build/report_generator
```

The generator:
1. Queries all active user IDs.
2. For each user, aggregates 7-day uptime %, downtime incidents, and average TTFB.
3. Exports a CSV to `REPORTS_DIR/`.
4. Generates an HTML report and converts it to PDF via `wkhtmltopdf` (graceful fallback to CSV if unavailable).
5. Saves report metadata to the `weekly_reports` table.
6. POSTs the report path to `INTERNAL_API_HOST/api/internal/send-report`.

---

### OS Cron Setup (Sunday 23:59 PM)

```bash
# Register the weekly cron job
chmod +x scripts/setup_cron.sh
./scripts/setup_cron.sh

# Verify installation
crontab -l | grep webnotifier

# Expected output:
# 59 23 * * 0  set -a && source ".../configs/.env" && ... # webnotifier-report-generator
```

---

### Run Unit Tests

```bash
./build/test_scheduler
```

Tests do **not** require a live database. All 16 tests run against in-memory
strategy objects and intentionally broken connection strings to verify
null-safety, divide-by-zero guards, and graceful empty-data handling.

---

### SQL Views Applied

```bash
# Apply Rishita's analytical views (read-only, safe to run on existing DB)
psql -U postgres -d webnotifier \
     -f database/migrations/004_analytics_views.sql
```

Views created:
- `v_downtime_incidents_7d` — incident count per website (last 7 days)
- `v_avg_response_7d` — average TTFB per website (last 7 days)
- `v_weekly_summary` — combined stats per website, per user
- `v_user_weekly_aggregate` — executive summary row per user

---

### OS & DBMS Concepts Demonstrated

| Concept | Where |
|---|---|
| Daemon thread (`std::thread`) | `Scheduler::run()` |
| Mutex (`std::mutex`) | `Scheduler::websites_` hot-reload |
| Exponential backoff | `Scheduler::connect_with_retry()` |
| Interruptible sleep (per-second tick) | `Scheduler::run()` sleep loop |
| Task pacing (50ms yield) | `Scheduler::run()` push loop |
| Signal handling (`SIGTERM/SIGINT`) | `scheduler_main.cpp` |
| RAII (`PGResultGuard`) | `analytics_engine.cpp` |
| `popen()` sub-process | `AnalyticsEngine::export_to_pdf()` |
| `libcurl` HTTP POST | `AnalyticsEngine::dispatch_report_email()` |
| `std::filesystem` | Reports dir creation |
| Parameterized queries (`PQexecParams`) | All SQL — prevents injection |
| Aggregation (`COUNT, AVG, SUM`) | `get_uptime_stats()`, views |
| `GROUP BY` + `LEFT JOIN` | `get_all_stats()` |
| Rolling window (`NOW() - INTERVAL`) | All 7-day queries |
| `NULLIF` divide-by-zero guard | `v_weekly_summary` view |
| `INSERT … ON CONFLICT DO NOTHING` | `save_report_to_db()` |
| Cron daemon | `setup_cron.sh` |
| `nohup` background process | `start_scheduler.sh` |
| PID file process management | `start/stop_scheduler.sh` |

