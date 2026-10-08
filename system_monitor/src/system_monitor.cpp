// ============================================================
// system_monitor/src/system_monitor.cpp
// OS System Resource Monitor — Implementation
// Owner: Simran Negi
//
// OS Concepts Demonstrated:
//   - /proc/stat parsing     : CPU jiffie delta between two reads
//   - sysinfo() system call  : kernel-level RAM statistics
//   - /proc/<pid>/comm       : process name lookup without exec
//   - /proc/<pid>/status     : per-process thread count
//   - Signal handling        : SIGINT/SIGTERM → graceful exit
//   - std::this_thread::sleep_for : interruptible sleep loop
//
// DBMS Concepts:
//   - Parameterized INSERT INTO system_metrics
//   - COUNT(*) on monitoring_jobs for live queue depth
//   - DELETE with time-window for table retention
//   - Exponential backoff on DB reconnect
// ============================================================
#include "../include/system_monitor.h"
#include<algorithm>
#include <libpq-fe.h>

// OS headers
#include <sys/sysinfo.h>     // sysinfo() system call
#include <dirent.h>          // opendir / readdir for /proc scanning
#include <unistd.h>          // getpid

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>           // strerror
#include <cerrno>

namespace webnotifier {

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
SystemMonitor::SystemMonitor(const std::string& db_conn_string,
                             int interval_sec,
                             const std::string& process_name)
    : db_conn_string_(db_conn_string),
      interval_sec_(interval_sec),
      process_name_(process_name),
      running_(false) {}

SystemMonitor::~SystemMonitor() {
    // RAII: always release DB connection
    if (pg_conn_) {
        PQfinish(static_cast<PGconn*>(pg_conn_));
        pg_conn_ = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public: run() — main sampling loop
// ─────────────────────────────────────────────────────────────────────────────
void SystemMonitor::run() {
    std::cout << "[SysMonitor] Connecting to database...\n";
    if (!connect_with_retry()) {
        std::cerr << "[SysMonitor] WARNING: DB unavailable. "
                     "Will retry each cycle.\n";
    }

    running_.store(true);
    int sample_count = 0;

    std::cout << "[SysMonitor] Starting sampling loop "
              << "(interval=" << interval_sec_ << "s, "
              << "watching='" << process_name_ << "')\n";

    // ── Read initial CPU times before first sleep ─────────────
    // OS Concept: /proc/stat must be read TWICE with a gap to
    // calculate a meaningful delta — identical to how `top` works.
    CpuTimes prev = read_cpu_times();

    while (running_.load()) {

        // ── Sleep for interval (interruptible per second) ─────
        for (int s = 0; s < interval_sec_ && running_.load(); ++s) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running_.load()) break;

        // ── Reconnect if DB connection dropped ────────────────
        if (!pg_conn_ ||
            PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK) {
            std::cerr << "[SysMonitor] DB connection lost. Reconnecting...\n";
            connect_with_retry();
        }

        // ─────────────────────────────────────────────────────
        // OS Concept 1: CPU usage via /proc/stat delta
        //
        // /proc/stat line format (first line is aggregate):
        //   cpu  <user> <nice> <system> <idle> <iowait> <irq> <softirq> <steal>
        //
        // We read it twice (before sleep and after) and compute:
        //   delta_total    = curr.total()    - prev.total()
        //   delta_non_idle = curr.non_idle() - prev.non_idle()
        //   cpu_pct = (delta_non_idle / delta_total) * 100
        // ─────────────────────────────────────────────────────
        CpuTimes curr   = read_cpu_times();
        double cpu_pct  = compute_cpu_pct(prev, curr);
        prev = curr;   // roll forward for next sample

        // ─────────────────────────────────────────────────────
        // OS Concept 2: RAM usage via sysinfo() system call
        //
        // sysinfo() fills a struct sysinfo with:
        //   totalram : total usable RAM bytes
        //   freeram  : free RAM bytes
        //   bufferram: RAM used by kernel buffers
        // ─────────────────────────────────────────────────────
        double mem_pct = get_mem_usage_pct();

        // ─────────────────────────────────────────────────────
        // OS Concept 3: Process thread count via /proc/<pid>/status
        //
        // Scan /proc for the scheduler_daemon process,
        // then read its Threads: field.
        // ─────────────────────────────────────────────────────
        int pid     = find_process_pid(process_name_);
        int threads = (pid > 0) ? get_thread_count(pid) : 0;

        // ─────────────────────────────────────────────────────
        // DBMS: queue depth from monitoring_jobs
        // ─────────────────────────────────────────────────────
        int queue = get_queue_depth();

        SystemSnapshot snap {
            cpu_pct,
            mem_pct,
            threads,
            queue
        };

        // Write to DB
        if (!insert_metric(snap)) {
            std::cerr << "[SysMonitor] Insert failed — skipping sample.\n";
        } else {
            std::cout << "[SysMonitor] "
                      << "CPU=" << cpu_pct   << "%  "
                      << "MEM=" << mem_pct   << "%  "
                      << "Threads=" << threads << "  "
                      << "Queue="   << queue   << "\n";
        }

        // Periodic cleanup
        ++sample_count;
        if (sample_count % CLEANUP_EVERY_N == 0) {
            cleanup_old_metrics(24);
        }
    }

    std::cout << "[SysMonitor] Stopped cleanly.\n";
}

void SystemMonitor::stop() {
    running_.store(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// OS Concept: Read /proc/stat
//
// /proc/stat is a virtual file created by the Linux kernel in the
// "proc" virtual filesystem. It is NOT stored on disk — the kernel
// generates it on-the-fly when any process opens it.
//
// First line example:
//   cpu  123456 678 90123 4567890 1234 0 567 0 0 0
//         user  nice system  idle iowait irq softirq steal
// ─────────────────────────────────────────────────────────────────────────────
CpuTimes SystemMonitor::read_cpu_times() const {
    CpuTimes t{};
    std::ifstream stat("/proc/stat");
    if (!stat.is_open()) {
        std::cerr << "[SysMonitor] WARNING: Cannot open /proc/stat\n";
        return t;
    }
    std::string label;
    stat >> label   // "cpu"
         >> t.user >> t.nice >> t.system >> t.idle
         >> t.iowait >> t.irq >> t.softirq >> t.steal;
    return t;
}

double SystemMonitor::compute_cpu_pct(const CpuTimes& t1,
                                       const CpuTimes& t2) const {
    long long delta_total    = t2.total()    - t1.total();
    long long delta_non_idle = t2.non_idle() - t1.non_idle();

    if (delta_total <= 0) return 0.0;

    double pct = (static_cast<double>(delta_non_idle)
                  / static_cast<double>(delta_total)) * 100.0;

    // Clamp to [0, 100]
    if (pct < 0.0)   pct = 0.0;
    if (pct > 100.0) pct = 100.0;
    return pct;
}

// ─────────────────────────────────────────────────────────────────────────────
// OS Concept: sysinfo() system call
//
// sysinfo() is Linux syscall #99 (on x86-64). It is the canonical
// way to query total/free/shared/buffer RAM without reading /proc.
// The kernel fills a `struct sysinfo` directly in kernel space,
// then copies it to the user-space pointer we provide.
// ─────────────────────────────────────────────────────────────────────────────
double SystemMonitor::get_mem_usage_pct() const {
    struct sysinfo info{};
    if (sysinfo(&info) != 0) {
        std::cerr << "[SysMonitor] sysinfo() failed: "
                  << strerror(errno) << "\n";
        return 0.0;
    }

    // totalram and freeram are in units of mem_unit bytes
    unsigned long long total = static_cast<unsigned long long>(info.totalram)
                               * info.mem_unit;
    unsigned long long free  = static_cast<unsigned long long>(info.freeram)
                               * info.mem_unit;
    // Subtract buffers from "used" — matches how `free` command does it
    unsigned long long buffers = static_cast<unsigned long long>(info.bufferram)
                                 * info.mem_unit;
    unsigned long long used  = total - free - buffers;

    if (total == 0) return 0.0;
    return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// OS Concept: Scanning /proc for a process by name
//
// /proc is a virtual filesystem. Every numeric directory /proc/<pid>/
// represents a running process. /proc/<pid>/comm contains the process
// command name (truncated to 15 chars by the kernel).
// ─────────────────────────────────────────────────────────────────────────────
int SystemMonitor::find_process_pid(const std::string& name) const {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        std::cerr << "[SysMonitor] Cannot open /proc: "
                  << strerror(errno) << "\n";
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        // Skip non-numeric entries (not PIDs)
        std::string dirname(entry->d_name);
        bool is_pid = !dirname.empty() &&
                      std::all_of(dirname.begin(), dirname.end(), ::isdigit);
        if (!is_pid) continue;

        // Read /proc/<pid>/comm
        std::string comm_path = "/proc/" + dirname + "/comm";
        std::ifstream comm_file(comm_path);
        if (!comm_file.is_open()) continue;

        std::string comm_name;
        std::getline(comm_file, comm_name);

        // comm is limited to 15 chars by the kernel — check prefix
        if (name.substr(0, 15) == comm_name.substr(0, 15)) {
            closedir(proc_dir);
            return std::stoi(dirname);
        }
    }
    closedir(proc_dir);
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// OS Concept: Reading /proc/<pid>/status
//
// /proc/<pid>/status is a human-readable file listing process metadata.
// The "Threads:" field tells us how many OS threads the process has.
// ─────────────────────────────────────────────────────────────────────────────
int SystemMonitor::get_thread_count(int pid) const {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream f(path);
    if (!f.is_open()) return 0;

    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Threads:", 0) == 0) {
            // line looks like: "Threads:\t8"
            std::istringstream iss(line);
            std::string key;
            int count = 0;
            iss >> key >> count;
            return count;
        }
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// DB: connect with exponential backoff
// ─────────────────────────────────────────────────────────────────────────────
bool SystemMonitor::connect_with_retry() {
    if (pg_conn_) {
        PQfinish(static_cast<PGconn*>(pg_conn_));
        pg_conn_ = nullptr;
    }

    int delay = 1;
    for (int attempt = 1; attempt <= MAX_DB_RETRIES; ++attempt) {
        PGconn* conn = PQconnectdb(db_conn_string_.c_str());
        if (PQstatus(conn) == CONNECTION_OK) {
            pg_conn_ = conn;
            std::cout << "[SysMonitor] DB connected (attempt "
                      << attempt << ").\n";
            return true;
        }
        std::cerr << "[SysMonitor] DB connect attempt " << attempt
                  << " failed: " << PQerrorMessage(conn)
                  << " — retrying in " << delay << "s...\n";
        PQfinish(conn);

        for (int s = 0; s < delay && running_.load(); ++s)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!running_.load()) return false;

        delay = std::min(delay * 2, MAX_BACKOFF_SEC);
    }
    std::cerr << "[SysMonitor] Giving up DB connection after "
              << MAX_DB_RETRIES << " attempts.\n";
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// DBMS: queue depth — COUNT pending monitoring_jobs
// ─────────────────────────────────────────────────────────────────────────────
int SystemMonitor::get_queue_depth() const {
    if (!pg_conn_ ||
        PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK)
        return 0;

    PGresult* res = PQexec(
        static_cast<PGconn*>(pg_conn_),
        "SELECT COUNT(*) FROM monitoring_jobs WHERE status = 'pending'");

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return 0;
    }
    int depth = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return depth;
}

// ─────────────────────────────────────────────────────────────────────────────
// DBMS: INSERT into system_metrics (parameterized — no SQL injection)
// ─────────────────────────────────────────────────────────────────────────────
bool SystemMonitor::insert_metric(const SystemSnapshot& snap) {
    if (!pg_conn_ ||
        PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK)
        return false;

    std::string cpu_s     = std::to_string(snap.cpu_usage_pct);
    std::string mem_s     = std::to_string(snap.mem_usage_pct);
    std::string threads_s = std::to_string(snap.active_threads);
    std::string queue_s   = std::to_string(snap.queue_depth);

    const char* params[] = {
        cpu_s.c_str(), mem_s.c_str(),
        threads_s.c_str(), queue_s.c_str()
    };

    PGresult* res = PQexecParams(
        static_cast<PGconn*>(pg_conn_),
        "INSERT INTO system_metrics "
        "  (cpu_usage_pct, mem_usage_pct, active_threads, queue_depth, recorded_at) "
        "VALUES ($1::numeric, $2::numeric, $3::int, $4::int, NOW())",
        4, nullptr, params, nullptr, nullptr, 0);

    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!ok) {
        std::cerr << "[SysMonitor] INSERT failed: "
                  << PQerrorMessage(static_cast<PGconn*>(pg_conn_)) << "\n";
    }
    PQclear(res);
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// DBMS: DELETE old rows — keeps table bounded
// ─────────────────────────────────────────────────────────────────────────────
void SystemMonitor::cleanup_old_metrics(int retention_hours) {
    if (!pg_conn_ ||
        PQstatus(static_cast<PGconn*>(pg_conn_)) != CONNECTION_OK)
        return;

    std::string hours_s = std::to_string(retention_hours);
    const char* params[] = { hours_s.c_str() };

    PGresult* res = PQexecParams(
        static_cast<PGconn*>(pg_conn_),
        "DELETE FROM system_metrics "
        "WHERE recorded_at < NOW() - ($1 || ' hours')::interval",
        1, nullptr, params, nullptr, nullptr, 0);

    if (PQresultStatus(res) == PGRES_COMMAND_OK)
        std::cout << "[SysMonitor] Cleaned up old metrics "
                     "(older than " << retention_hours << "h).\n";
    PQclear(res);
}

} // namespace webnotifier
