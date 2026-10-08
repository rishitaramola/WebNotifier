// ============================================================
// system_monitor/include/system_monitor.h
// OS System Resource Monitor — Header
// Owner: Simran Negi
//
// OS Concepts Demonstrated:
//   - /proc/stat parsing     → CPU usage (Linux virtual filesystem)
//   - sysinfo() system call  → RAM usage (sys/sysinfo.h)
//   - /proc/<pid>/status     → Thread count of scheduler_daemon
//   - Signal handling        → SIGINT / SIGTERM graceful shutdown
//   - Daemon loop            → background sampling with sleep_for
// ============================================================
#pragma once

#include <string>
#include <atomic>

namespace webnotifier {

// ── Data Structures ──────────────────────────────────────────────────────────

/**
 * @brief Snapshot of system resource usage at one point in time.
 * Written to the system_metrics table by SystemMonitor.
 */
struct SystemSnapshot {
    double  cpu_usage_pct;    ///< System-wide CPU usage 0–100
    double  mem_usage_pct;    ///< RAM used / total * 100
    int     active_threads;   ///< Thread count of scheduler_daemon process
    int     queue_depth;      ///< Pending rows in monitoring_jobs table
};

// ── CPU stat helpers (reads /proc/stat) ──────────────────────────────────────

/**
 * @brief Raw CPU time counters read from /proc/stat.
 * OS Concept: /proc/stat is a virtual file maintained by the Linux kernel
 * that exposes cumulative CPU jiffies since boot.
 */
struct CpuTimes {
    long long user, nice, system, idle, iowait, irq, softirq, steal;
    long long total()    const { return user+nice+system+idle+iowait+irq+softirq+steal; }
    long long non_idle() const { return user+nice+system+irq+softirq+steal; }
};

// ── SystemMonitor class ───────────────────────────────────────────────────────

/**
 * @brief Background daemon that samples OS metrics and writes to PostgreSQL.
 *
 * OS Concepts:
 *   - Reads /proc/stat          for CPU usage delta between two samples
 *   - Calls sysinfo()           for total/free RAM
 *   - Reads /proc/<pid>/status  for scheduler_daemon thread count
 *   - Handles SIGINT/SIGTERM    for graceful shutdown
 *   - Daemon loop with          std::this_thread::sleep_for
 */
class SystemMonitor {
public:
    /**
     * @param db_conn_string  libpq connection string (from env vars).
     * @param interval_sec    Sampling interval in seconds (default 30).
     * @param process_name    Name of C++ process to watch (default: scheduler_daemon).
     */
    explicit SystemMonitor(const std::string& db_conn_string,
                           int interval_sec   = 30,
                           const std::string& process_name = "scheduler_daemon");
    ~SystemMonitor();

    // Non-copyable (owns a PGconn*)
    SystemMonitor(const SystemMonitor&)            = delete;
    SystemMonitor& operator=(const SystemMonitor&) = delete;

    /** @brief Connect to DB and start the sampling loop (blocks until stopped). */
    void run();

    /** @brief Signal the loop to stop (called from signal handler). */
    void stop();

private:
    // ── OS metric readers ────────────────────────────────────────
    /**
     * @brief Read current CPU time counters from /proc/stat.
     * OS: /proc/stat is a virtual file in the Linux procfs.
     * Each line starting with "cpu" contains cumulative jiffies.
     */
    CpuTimes read_cpu_times() const;

    /**
     * @brief Compute CPU usage % between two /proc/stat reads.
     * Formula: (delta_non_idle / delta_total) * 100
     */
    double compute_cpu_pct(const CpuTimes& t1, const CpuTimes& t2) const;

    /**
     * @brief Get RAM usage % via sysinfo() system call.
     * OS: sysinfo() is a Linux system call (syscall number 99 on x86-64).
     * Returns struct sysinfo with totalram, freeram, bufferram etc.
     */
    double get_mem_usage_pct() const;

    /**
     * @brief Find PID of process_name_ by scanning /proc/<pid>/comm.
     * OS: /proc/<pid>/comm contains the process name (first 15 chars).
     * Returns -1 if not found.
     */
    int find_process_pid(const std::string& name) const;

    /**
     * @brief Read thread count from /proc/<pid>/status.
     * OS: /proc/<pid>/status exposes per-process info including Threads field.
     * Returns 0 if PID not found or /proc entry is unreadable.
     */
    int get_thread_count(int pid) const;

    // ── DB helpers ───────────────────────────────────────────────
    /**
     * @brief Connect to PostgreSQL with exponential backoff retry.
     * @return true on success.
     */
    bool connect_with_retry();

    /**
     * @brief Count pending rows in monitoring_jobs as queue depth.
     * DBMS: SELECT COUNT(*) WHERE status = 'pending'
     */
    int get_queue_depth() const;

    /**
     * @brief INSERT one row into system_metrics.
     * DBMS: Parameterized INSERT — prevents SQL injection.
     */
    bool insert_metric(const SystemSnapshot& snap);

    /**
     * @brief DELETE old rows to keep the table lean.
     * DBMS: DELETE WHERE recorded_at < NOW() - INTERVAL
     * @param retention_hours Keep only this many hours of history.
     */
    void cleanup_old_metrics(int retention_hours = 24);

    // ── Data members ─────────────────────────────────────────────
    std::string         db_conn_string_;
    int                 interval_sec_;
    std::string         process_name_;
    void*               pg_conn_{ nullptr };   ///< PGconn* (opaque)
    std::atomic<bool>   running_{ false };

    static constexpr int MAX_DB_RETRIES  = 6;
    static constexpr int MAX_BACKOFF_SEC = 64;
    static constexpr int CLEANUP_EVERY_N = 20; ///< cleanup every N samples
};

} // namespace webnotifier
