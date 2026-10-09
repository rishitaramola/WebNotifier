# Project Structure

WebNotifier is a **module-grouped monorepo**: each top-level folder is one component,
grouped by responsibility. This keeps OS modules (C++) and web modules (Node/JS)
separate while sharing one database.

```
WebNotifier/
├── backend/              # Node.js REST API (Express) — auth, websites, analytics endpoints
│   ├── src/
│   │   ├── server.js         # app entry, CORS, session, route mounting
│   │   ├── db.js             # pg connection pool
│   │   ├── routes/           # auth, websites, analytics, alerts, systemMetrics
│   │   ├── middleware/       # auth (session guard)
│   │   └── utils/            # mailer (nodemailer)
│   ├── .env.example          # Node config template
│   └── package.json
│
├── frontend/             # Static dashboard (no build step)
│   ├── index.html
│   ├── css/style.css
│   └── js/                   # api.js (fetch client), app.js, charts.js, bundle.js
│
├── queue/                # C++ thread-safe task queue (producer/consumer)
│   ├── include/              # task.h, task_queue.h, monitoring_result.h
│   └── src/task_queue.cpp
│
├── scheduler/            # C++ scheduler daemon (decides which sites are "due")
│   ├── include/              # scheduler.h, scheduling_strategy.h
│   └── src/                  # scheduler.cpp, scheduler_main.cpp, report_generator_main.cpp
│
├── worker_pool/          # C++ worker threads — HTTP checks + DB writes
│   ├── include/              # worker_pool.h, network_checker.h, db_writer.h
│   └── src/                  # worker_pool.cpp, network_checker.cpp, db_writer.cpp
│
├── analytics/            # C++ analytics engine (weekly reports, aggregations)
│   ├── include/ · src/
│
├── system_monitor/       # C++ host resource monitor daemon (CPU/mem metrics)
│   ├── include/ · src/
│
├── database/             # SQL — single source of truth for the schema
│   ├── schema/               # 001_create_tables, 002_create_indexes
│   ├── views/                # dashboard + reporting views
│   ├── migrations/           # incremental changes (003, 004)
│   └── seeds/                # seed_data.sql (demo rows)
│
├── tests/                # All C++ tests live here (consolidated)
│   ├── scheduler/            # test_scheduler.cpp
│   └── worker_pool/          # test_worker_pool.cpp, test_db_writer.cpp
│
├── scripts/              # start/stop daemon, cron setup
├── configs/              # .env.example for the C++ side
├── CMakeLists.txt        # builds all C++ targets
├── RUN.md                # how to run everything
└── README.md             # project overview + OS/DBMS concepts
```

## Data flow

```
Frontend (5500) ──HTTP──> Backend API (8080) ──SQL──> PostgreSQL (webnotifier)
                                                          ▲
                                                          │ writes monitoring_results
Scheduler ──Task──> Queue ──> Worker Pool ──HTTP check──> │
(every 60s)         (C++)      (4 threads)                │
                                                          │ reads
Analytics / weekly reports ───────────────────────────────┘
```

## Build targets (CMake)

| Target | Source | Output |
|--------|--------|--------|
| `scheduler_daemon` | `scheduler/src/scheduler_main.cpp` | Scheduler + Worker Pool in one process |
| `report_generator` | `scheduler/src/report_generator_main.cpp` | Weekly CSV/PDF reports |
| `system_monitor_daemon` | `system_monitor/src/system_monitor_main.cpp` | Host metrics collector |
| `test_scheduler` | `tests/scheduler/test_scheduler.cpp` | Scheduler unit tests |
| `test_worker_pool` | `tests/worker_pool/test_worker_pool.cpp` | Worker pool tests |

## Conventions

- **C++ modules** follow `include/` (headers) + `src/` (implementation) per folder.
- **One database**; all schema changes go through `database/migrations/` in order.
- **Tests** are centralized under `tests/<module>/`, not beside the source.
- **Secrets** live in `.env` files, never committed (`*.env` is git-ignored; `*.env.example` is the template).
