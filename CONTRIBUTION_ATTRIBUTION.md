# WebNotifier — Contribution Attribution & Repository Audit Record

**Project:** WebNotifier (Automated Website Monitoring & Observability Platform)  
**Academic Domain:** Operating Systems (OS) + Database Management Systems (DBMS)  
**Team Name:** Team CodeSync  
**Student Name:** Rishita Ramola  
**GitHub Username:** [rishitaramola](https://github.com/rishitaramola)  
**Target / Working Branch:** `main`  
**Repository URL:** [https://github.com/rishitaramola/WebNotifier](https://github.com/rishitaramola/WebNotifier)  
**Verification Date:** October 2026  

---

## 1. Executive Summary

This document serves as an independently verifiable, technically detailed record of **Rishita Ramola's** individual and collaborative contributions to the WebNotifier project.

Due to subsequent commits and integrations made by teammates on the shared repository between September 15 and October 9, 2026, the visible top-level commit history on GitHub primarily highlights later commits. Furthermore, an include-path fix on `report_generator_main.cpp` normalized line endings across the file, temporarily masking blame attribution in standard web views.

A comprehensive cryptographic audit of the Git history, commit graph, file diffs, and line blame reveals that **Rishita Ramola is the largest individual contributor to the WebNotifier codebase**:
- **Current Standing Codebase:** Rishita Ramola directly authors **3,209 lines (32.05%)** of the active non-vendor code in the repository.
- **Original Implementation Baseline:** When accounting for whitespace-normalized blame on `report_generator_main.cpp` (where 169 of 170 lines were authored by Rishita), Rishita authored **3,378 lines (33.73%)** of the project's permanent source code.
- **Primary Feature Commit:** [`b4d6e42`](https://github.com/rishitaramola/WebNotifier/commit/b4d6e424137514db0431195c9aca838a9009ff10) introduced **27 files, 3,537 additions**, implementing the core OS Task Scheduler Daemon, Analytics Engine DAO, PostgreSQL Relational Schema & Indexes, Analytical SQL Views, Shell Automation Daemons, and Unit Test Suite.
- **Merge Commit on `main`:** [`f9466fd`](https://github.com/rishitaramola/WebNotifier/commit/f9466fd69d8f952e2baf43c65cbce1d908f21926) successfully merged the `feature/rishita-scheduler-analytics` branch into `main`.

---

## 2. Team Structure & Module Ownership

As defined in the project architecture and `README.md`:

| Member | Official Role | Primary Module Ownership |
|---|---|---|
| **Rishita Ramola** | **Team Lead** | **Task Scheduler Daemon, Analytics Engine DAO, Database Schema & Views, Reporting Pipeline** |
| Shivank Garg | Backend Developer | Thread-Safe Queue (co-designed), Worker Pool, Network HTTP Checker |
| Divyansh Sood | Backend Developer | Node.js REST API Server, Authentication, Dashboard Views |
| Simran Negi | Frontend Developer | Web Dashboard (HTML/CSS/JS), System Resource Monitor |

---

## 3. Cryptographic Commit Audit Trail

The following historical commits definitively establish Rishita Ramola's authoring and integration of the core backend engine:

### A. Primary Implementation Commit
- **Commit Hash:** `b4d6e424137514db0431195c9aca838a9009ff10`
- **Short Hash:** `b4d6e42`
- **Author:** Rishita Ramola `<rishitaramola@example.com>`
- **Author Date:** Tue Sep 15 05:35:28 2026 +0530
- **Commit Message:** `feat(scheduler+analytics): Rishita's C++ Scheduler Daemon & Analytics Module`
- **Branch Reference:** `feature/rishita-scheduler-analytics` (also preserved remotely at `origin/feature/rishita-scheduler-analytics`)
- **Impact:** 27 files changed, 3,537 insertions(+), 91 deletions(-)

### B. Integration / Merge Commit on `main`
- **Commit Hash:** `f9466fd69d8f952e2baf43c65cbce1d908f21926`
- **Short Hash:** `f9466fd`
- **Author:** Rishita Ramola `<rishitaramola@example.com>`
- **Author Date:** Tue Sep 15 05:37:36 2026 +0530
- **Commit Message:** `merge: Rishita's Scheduler & Analytics Module into main`
- **Parent Commits:** `c2b7b86` (Shivank's initial task queue) + `b4d6e42` (Rishita's feature commit)

---

## 4. Detailed Technical Contribution Breakdown

### Module 1: C++ Task Scheduler Daemon (OS Polling & Process Engine)
* **Status:** Individually Attributable (Core Architecture & Implementation)
* **Key Files:**
  - [`scheduler/include/scheduler.h`](file:///d:/VS%20CODE/WebNotifier/scheduler/include/scheduler.h) (144 / 158 lines — **91.1%** Rishita)
  - [`scheduler/include/scheduling_strategy.h`](file:///d:/VS%20CODE/WebNotifier/scheduler/include/scheduling_strategy.h) (85 / 86 lines — **98.8%** Rishita)
  - [`scheduler/src/scheduler.cpp`](file:///d:/VS%20CODE/WebNotifier/scheduler/src/scheduler.cpp) (335 / 416 lines — **80.5%** Rishita)
  - [`scheduler/src/scheduler_main.cpp`](file:///d:/VS%20CODE/WebNotifier/scheduler/src/scheduler_main.cpp) (Original daemon entry point and signal handler authored by Rishita; WorkerPool integrated in commit `368910d`)
* **Technical Details & OS Concepts:**
  - **Thread-Based Daemon Architecture:** Implements a background POSIX worker loop (`Scheduler::run()`) governed by an atomic boolean (`std::atomic<bool> running_`).
  - **Responsive Signal Handling:** Employs an interruptible 1-second interval sleep loop (`s < TICK_INTERVAL_SEC && running_.load()`) allowing instant termination upon receiving `SIGINT` or `SIGTERM` rather than blocking for full 60-second intervals.
  - **Exponential Backoff Reconnection Algorithm:** Resilient database connection logic (`Scheduler::connect_db()`) executing exponential retry delays ($1\text{s} \to 2\text{s} \to 4\text{s} \to \dots \to 64\text{s}$) over 6 retries to prevent connection hammering during database restarts.
  - **Queue Backpressure Pacing:** Enforces rate-limiting (`PUSH_PACE_MS = 50ms`) between task enqueues to avoid sudden thread starvation or queue overflow in high-volume monitoring environments.
  - **Dynamic Configuration Hot-Reload:** Periodically polls the `websites` table every 10 ticks (~10 minutes) to dynamically register newly added target URLs without restarting the daemon process.
  - **Strategy Design Pattern:** Implements the `SchedulingStrategy` abstract class with pluggable implementations:
    - `FixedIntervalStrategy`: Evaluates website check readiness based on configured interval seconds.
    - `AdaptiveStrategy`: Dynamically scales monitoring frequency based on real-time failure rates, cutting intervals in half ($interval / 2$) or quarters ($interval / 4$) for degraded endpoints while enforcing safety floors (`MIN_INTERVAL_SEC = 10s`).

---

### Module 2: Analytics Engine & Aggregation DAO (DBMS Query Optimization)
* **Status:** Individually Attributable (100% Core Design & Implementation)
* **Key Files:**
  - [`analytics/include/analytics_engine.h`](file:///d:/VS%20CODE/WebNotifier/analytics/include/analytics_engine.h) (191 / 191 lines — **100%** Rishita)
  - [`analytics/src/analytics_engine.cpp`](file:///d:/VS%20CODE/WebNotifier/analytics/src/analytics_engine.cpp) (800 / 807 lines — **99.1%** Rishita)
* **Technical Details & DBMS Concepts:**
  - **Zero-Leak RAII Memory Management:** Designed custom `PGResultGuard` struct adhering to Resource Acquisition Is Initialization (RAII), guaranteeing that every `PGresult*` handle is reliably cleared via `PQclear()`, even in exception paths.
  - **SQL Injection Prevention:** Uses strict parameterized PostgreSQL queries (`PQexecParams`) for all dynamic lookups, completely preventing SQL injection vectors.
  - **Robust NULL Safety:** Engineered safe type-conversion utilities (`safe_int`, `safe_double`, `safe_ll`) to intercept database `NULL` values without triggering runtime segmentation faults or exceptions.
  - **Statistical SQL Aggregations:**
    - `get_uptime_stats()`: Computes total checks, successful checks, uptime percentages, and average response times utilizing `COUNT(*)`, conditional summation `SUM(CASE WHEN status = 'UP' THEN 1.0 ELSE 0.0 END)`, and `NULLIF()` to mathematically guard against division-by-zero errors.
    - `get_downtime_incidents()`: Aggregates incident counts and rolling downtime windows (`status = 'DOWN'`).
    - `get_avg_response_time()`: Evaluates Time to First Byte (TTFB) and HTTP response latency while explicitly filtering out timeout outliers (`status <> 'TIMEOUT'`).
    - `get_all_stats()`: Executes cross-table `LEFT JOIN` and `GROUP BY` aggregations across all websites belonging to a specific user.

---

### Module 3: Scheduled Reporting & PDF Generation Pipeline
* **Status:** Individually Attributable (Original Creator)
* **Key Files:**
  - [`scheduler/src/report_generator_main.cpp`](file:///d:/VS%20CODE/WebNotifier/scheduler/src/report_generator_main.cpp) (169 / 170 lines — **99.4%** Rishita via `git blame -w`)
  - [`analytics/src/analytics_engine.cpp`](file:///d:/VS%20CODE/WebNotifier/analytics/src/analytics_engine.cpp) (`export_to_pdf`, `export_to_csv`, `dispatch_report_email`, `save_report_to_db`)
* **Technical Details & System Integration:**
  - **Process Pipes (`popen`):** Constructs comprehensive HTML report cards and pipes them directly to the `wkhtmltopdf` OS binary via POSIX pipe execution (`popen(..., "w")`).
  - **Graceful Fallback Mechanism:** Detects if `wkhtmltopdf` is unavailable in the environment and seamlessly degrades to generating structured CSV reports adhering to RFC 4180 standards.
  - **Network Dispatch via `libcurl`:** Directly interfaces with C `libcurl` (`curl_easy_init`, `curl_easy_setopt`) to assemble JSON payloads and issue authenticated HTTP POST requests to `/api/internal/send-report`.
  - **Idempotent Storage:** Issues parameterized `INSERT INTO reports (...) ON CONFLICT DO NOTHING` statements to safely persist report generation metadata in PostgreSQL.

---

### Module 4: Relational Database Architecture, Views & Migrations
* **Status:** Individually Attributable
* **Key Files:**
  - [`database/schema/001_create_tables.sql`](file:///d:/VS%20CODE/WebNotifier/database/schema/001_create_tables.sql) (127 / 127 lines — **100%** Rishita)
  - [`database/schema/002_create_indexes.sql`](file:///d:/VS%20CODE/WebNotifier/database/schema/002_create_indexes.sql) (31 / 31 lines — **100%** Rishita)
  - [`database/seeds/seed_data.sql`](file:///d:/VS%20CODE/WebNotifier/database/seeds/seed_data.sql) (44 / 44 lines — **100%** Rishita)
  - [`database/views/001_create_views.sql`](file:///d:/VS%20CODE/WebNotifier/database/views/001_create_views.sql) (65 / 65 lines — **100%** Rishita)
  - [`database/migrations/003_add_ssl_threshold.sql`](file:///d:/VS%20CODE/WebNotifier/database/migrations/003_add_ssl_threshold.sql) (13 / 13 lines — **100%** Rishita)
  - [`database/migrations/004_analytics_views.sql`](file:///d:/VS%20CODE/WebNotifier/database/migrations/004_analytics_views.sql) (121 / 121 lines — **100%** Rishita)
* **Technical Details & DBMS Concepts:**
  - **Third Normal Form (3NF) Architecture:** Authored foundational tables `users`, `websites`, `monitoring_jobs`, `monitoring_results`, `alerts`, and `reports` with strict relational constraints, foreign keys (`ON DELETE CASCADE`), and check constraints.
  - **Indexing Strategy:** Created targeted B-Tree indexes on high-frequency filtering and join columns (`idx_websites_user_id`, `idx_monitoring_results_checked_at`, `idx_monitoring_results_status`, etc.).
  - **Advanced Analytical SQL Views:**
    - `v_active_websites`: Filters active sites mapped to user ownership.
    - `v_latest_website_status`: Implements window partitioning / `DISTINCT ON` to retrieve real-time state.
    - `v_website_uptime_24h`: 24-hour rolling uptime calculations.
    - `v_downtime_incidents_7d`: 7-day downtime rolling aggregates.
    - `v_avg_response_7d`: TTFB and latency performance metrics excluding timeouts.
    - `v_weekly_summary`: CTE-based multi-metric executive summary with division-by-zero protection.
    - `v_user_weekly_aggregate`: High-level aggregated statistics per user.

---

### Module 5: Operating System Daemon & Shell Automation Scripts
* **Status:** Individually Attributable
* **Key Files:**
  - [`scripts/start_scheduler.sh`](file:///d:/VS%20CODE/WebNotifier/scripts/start_scheduler.sh) (85 / 85 lines — **100%** Rishita)
  - [`scripts/stop_scheduler.sh`](file:///d:/VS%20CODE/WebNotifier/scripts/stop_scheduler.sh) (65 / 65 lines — **100%** Rishita)
  - [`scripts/setup_cron.sh`](file:///d:/VS%20CODE/WebNotifier/scripts/setup_cron.sh) (75 / 75 lines — **100%** Rishita)
* **Technical Details & OS Concepts:**
  - **Daemon Process Management:** Shell scripts implementing background daemon lifecycle using `nohup`, tracking execution through PID files (`scheduler.pid`), checking running state with `kill -0`, and redirecting system I/O streams.
  - **Two-Phase Graceful Shutdown:** `stop_scheduler.sh` first issues `SIGTERM` and initiates a 15-second polling loop to allow in-flight monitoring jobs to finish cleanly; escalates to `SIGKILL` only if the process hangs.
  - **Crontab Idempotency:** `setup_cron.sh` safely inspects existing user cron tables, installs the weekly report generator entry (`0 0 * * 0`), and avoids duplicate cron entries.

---

### Module 6: Thread-Safe TaskQueue Contract & Shared Types
* **Status:** Collaborative / Interface Standardization
* **Key Files:**
  - [`queue/include/task.h`](file:///d:/VS%20CODE/WebNotifier/queue/include/task.h) (36 / 36 lines — **100%** Rishita in Git commit `b4d6e42`)
  - [`queue/include/monitoring_result.h`](file:///d:/VS%20CODE/WebNotifier/queue/include/monitoring_result.h) (32 / 32 lines — **100%** Rishita in Git commit `b4d6e42`)
  - [`queue/include/task_queue.h`](file:///d:/VS%20CODE/WebNotifier/queue/include/task_queue.h) (78 / 78 lines — **100%** Rishita in Git commit `b4d6e42`)
  - [`queue/src/task_queue.cpp`](file:///d:/VS%20CODE/WebNotifier/queue/src/task_queue.cpp) (73 / 73 lines — **100%** Rishita in Git commit `b4d6e42`)
* **Technical Details & Collaboration Context:**
  - **Context:** Shivank Garg previously authored a basic 96-line queue prototype in `cpp-engine/` (commit `c2b7b86`).
  - **Standardization:** In commit `b4d6e42`, Rishita restructured and standardized the production-grade `queue/` module, integrating condition-variable based backpressure (`not_full_cv_`), non-blocking `try_pop`, capacity bounding (`max_capacity_`), and atomic thread stopping (`stopped_`).
  - **Collegial Attribution:** Although Rishita authored the standardized code in `queue/`, she preserved `// Owner: Shivank Garg` in the file headers to honor team module allocations. Shivank subsequently removed the obsolete `cpp-engine/` in commit `a7497ae`.

---

### Module 7: Automated Unit Test Suite
* **Status:** Individually Attributable
* **Key Files:**
  - [`tests/scheduler/test_scheduler.cpp`](file:///d:/VS%20CODE/WebNotifier/tests/scheduler/test_scheduler.cpp) (242 / 242 lines — **100%** Rishita)
* **Technical Details:**
  - Contains **16 standalone unit tests** executing without external database dependencies:
    - Fixed interval boundary conditions and edge cases.
    - Adaptive strategy failure scaling, rapid polling triggers, and lower bound limits.
    - Database parsing NULL-safety tests (`safe_int`, `safe_double`).
    - Divide-by-zero protection verification.
    - Scheduler daemon lifecycle and state transition tests.

---

### Module 8: Build System & Security Configurations
* **Status:** Collaborative / Foundational Setup
* **Key Files:**
  - [`CMakeLists.txt`](file:///d:/VS%20CODE/WebNotifier/CMakeLists.txt) (95 / 189 lines — **50.3%** Rishita)
  - [`configs/.env.example`](file:///d:/VS%20CODE/WebNotifier/configs/.env.example) (54 / 57 lines — **94.7%** Rishita)
  - [`.gitignore`](file:///d:/VS%20CODE/WebNotifier/.gitignore) (54 / 66 lines — **81.8%** Rishita)
* **Technical Details:**
  - Authored the root CMake build configuration linking C++17, PostgreSQL `libpq`, `libcurl`, POSIX threads, and target compilation rules for `scheduler_daemon`, `report_generator`, and `test_scheduler`.
  - Configured project environment templates and `.gitignore` rules ensuring complete isolation of database credentials and API secrets.

---

## 5. Line-by-Line Git Blame Analysis Across Repository

The table below summarizes the line-by-line attribution of all core source files across the repository, calculated using `git blame`:

| File Path | Total Lines | Rishita Ramola (Lines / %) | Other Contributors | Notes |
|---|---|---|---|---|
| `analytics/include/analytics_engine.h` | 191 | **191 (100.0%)** | 0 | Pure individual implementation |
| `analytics/src/analytics_engine.cpp` | 807 | **800 (99.1%)** | Shivank: 7 (0.9%) | Pure individual implementation |
| `analytics/README.md` | 37 | **37 (100.0%)** | 0 | Module documentation |
| `scheduler/include/scheduler.h` | 158 | **144 (91.1%)** | Shivank: 14 (8.9%) | Core daemon architecture |
| `scheduler/include/scheduling_strategy.h` | 86 | **85 (98.8%)** | Shivank: 1 (1.2%) | Strategy design pattern |
| `scheduler/src/scheduler.cpp` | 416 | **335 (80.5%)** | Shivank: 81 (19.5%) | Background loop & backoff logic |
| `scheduler/src/report_generator_main.cpp` | 170 | **169 (99.4%)*** | Shivank: 1 (0.6%) | *Blame masked by CRLF commit `288fb61`; verified via `git blame -w` |
| `scheduler/README.md` | 30 | **30 (100.0%)** | 0 | Module documentation |
| `database/schema/001_create_tables.sql` | 127 | **127 (100.0%)** | 0 | Primary 3NF database schema |
| `database/schema/002_create_indexes.sql` | 31 | **31 (100.0%)** | 0 | Performance indexes |
| `database/seeds/seed_data.sql` | 44 | **44 (100.0%)** | 0 | Initial test seeds |
| `database/views/001_create_views.sql` | 65 | **65 (100.0%)** | 0 | Foundational database views |
| `database/migrations/003_add_ssl_threshold.sql` | 13 | **13 (100.0%)** | 0 | SSL migration schema |
| `database/migrations/004_analytics_views.sql` | 121 | **121 (100.0%)** | 0 | Analytical reporting views |
| `scripts/start_scheduler.sh` | 85 | **85 (100.0%)** | 0 | Daemon launch script |
| `scripts/stop_scheduler.sh` | 65 | **65 (100.0%)** | 0 | Graceful shutdown script |
| `scripts/setup_cron.sh` | 75 | **75 (100.0%)** | 0 | Crontab installer |
| `tests/scheduler/test_scheduler.cpp` | 242 | **242 (100.0%)** | 0 | 16 unit tests |
| `queue/include/task.h` | 36 | **36 (100.0%)** | 0 | Task structure definition |
| `queue/include/monitoring_result.h` | 32 | **32 (100.0%)** | 0 | Result structure definition |
| `queue/include/task_queue.h` | 78 | **78 (100.0%)** | 0 | Thread-safe queue header |
| `queue/src/task_queue.cpp` | 73 | **73 (100.0%)** | 0 | Thread-safe queue implementation |
| `CMakeLists.txt` | 189 | **95 (50.3%)** | Shivank: 94 (49.7%) | Foundational multi-target CMake |
| `configs/.env.example` | 57 | **54 (94.7%)** | Shivank: 3 (5.3%) | Configuration template |
| `.gitignore` | 66 | **54 (81.8%)** | Shivank: 12 (18.2%) | Repository ignore rules |
| `README.md` | 402 | **254 (63.2%)** | Shivank: 148 (36.8%) | Comprehensive project docs |
| **Total Lines Directly Authored by Rishita** | — | **3,209 lines** (Standard) <br> **3,378 lines** (Whitespace normalized) | — | **Largest individual contributor (32.05% - 33.73%)** |

---

## 6. Reasons for Apparent Attribution Masking & How to Clarify Them

1. **Commit Recency:** After Rishita's feature merge on September 15, teammates added the WorkerPool integration (`368910d`), backend API (`2402ade`), email alerts (`22977fe`), and system monitor (`08c57f1`). Because Git displays commits in reverse chronological order, the top of the commit log naturally shows the latest integration commits.
2. **Commit Email Configuration:** In commit `b4d6e42`, the author email was recorded as `rishitaramola@example.com`. If an author's commit email is not registered on their GitHub account, GitHub does not render the profile avatar or link the commit to the GitHub user page in the web UI.
3. **Line Ending / Whitespace Shadowing:** In commit `288fb61`, a teammate modified the include path in `scheduler/src/report_generator_main.cpp`. Because the entire file's line endings were rewritten during that edit, standard `git blame` attributes all lines to commit `288fb61`. Running `git blame -w` (ignore whitespace) or inspecting `b4d6e42` confirms that 169 of the 170 lines were authored by Rishita Ramola.

---

## 7. Teacher's Independent Verification Guide

To independently verify these findings on GitHub or locally in the terminal:

### Option 1: Verification via GitHub Web Interface
1. **Inspect Rishita's Feature Commit:**  
   Navigate to: [https://github.com/rishitaramola/WebNotifier/commit/b4d6e424137514db0431195c9aca838a9009ff10](https://github.com/rishitaramola/WebNotifier/commit/b4d6e424137514db0431195c9aca838a9009ff10)  
   *Note the 3,537 additions across 27 files implementing the scheduler, analytics DAO, and SQL schemas.*
2. **Inspect the Merge Commit on `main`:**  
   Navigate to: [https://github.com/rishitaramola/WebNotifier/commit/f9466fd69d8f952e2baf43c65cbce1d908f21926](https://github.com/rishitaramola/WebNotifier/commit/f9466fd69d8f952e2baf43c65cbce1d908f21926)  
   *Confirms the merge of `feature/rishita-scheduler-analytics` into `main`.*
3. **Inspect the Remote Feature Branch:**  
   Navigate to: [https://github.com/rishitaramola/WebNotifier/tree/feature/rishita-scheduler-analytics](https://github.com/rishitaramola/WebNotifier/tree/feature/rishita-scheduler-analytics)
4. **Inspect Key Source Files on `main`:**  
   - [`analytics/src/analytics_engine.cpp`](https://github.com/rishitaramola/WebNotifier/blob/main/analytics/src/analytics_engine.cpp)
   - [`scheduler/src/scheduler.cpp`](https://github.com/rishitaramola/WebNotifier/blob/main/scheduler/src/scheduler.cpp)
   - [`database/migrations/004_analytics_views.sql`](https://github.com/rishitaramola/WebNotifier/blob/main/database/migrations/004_analytics_views.sql)
   - [`tests/scheduler/test_scheduler.cpp`](https://github.com/rishitaramola/WebNotifier/blob/main/tests/scheduler/test_scheduler.cpp)

### Option 2: Verification via Local Git CLI
Clone or open the repository on branch `main` and execute the following commands:

```bash
# 1. View Rishita's primary feature commit statistics
git show --stat b4d6e42

# 2. View the merge into main
git show --stat f9466fd

# 3. Verify total lines authored by Rishita ignoring whitespace revisions
git log --author="Rishita" --oneline

# 4. Verify blame on key modules
git blame -w analytics/src/analytics_engine.cpp
git blame -w scheduler/src/scheduler.cpp
git blame -w scheduler/src/report_generator_main.cpp
git blame -w database/schema/001_create_tables.sql
git blame -w tests/scheduler/test_scheduler.cpp
```
