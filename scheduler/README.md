# Scheduler Module
**Owner:** Rishita Ramola

## Purpose
Cron-like job dispatcher that reads website configs from the database,
determines which websites are due for a health check, and pushes Task
objects into the shared thread-safe TaskQueue.

## Files
| File | Description |
|---|---|
| `include/scheduler.h` | Scheduler class declaration |
| `include/scheduling_strategy.h` | Strategy pattern interface |
| `src/scheduler.cpp` | Implementation |

## OS Concepts
- **Daemon Thread**: runs `Scheduler::run()` in background via `std::thread`
- **Mutex**: protects `websites_` list during reload
- **Scheduling**: tick-based loop, sleeps 60 seconds between cycles

## How It Works
1. `start()` launches a daemon thread
2. Every 60 seconds, the loop wakes up
3. For each active website, checks if `(now - last_checked) >= interval`
4. If due, creates a `Task` and calls `queue_.push(task)`
5. `stop()` sets `running_ = false` and joins the thread

## Integration
- **Reads from**: PostgreSQL `websites` table
- **Writes to**: `TaskQueue` (shared with Worker Pool)
