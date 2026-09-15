// ============================================================
// queue/include/task.h
// Task struct - shared contract between Scheduler and Queue
// Owner: Shivank Garg
// ============================================================
#pragma once
#include <string>

namespace webnotifier {

/**
 * @brief Represents a single monitoring task.
 * Produced by the Scheduler and consumed by the Worker Pool.
 * This is the core data contract between all backend modules.
 */
struct Task {
    int         id;           // unique task/job ID from DB
    int         website_id;   // FK to websites table
    std::string url;          // URL to check
    std::string keyword;      // optional keyword to find in response body
    int         timeout_sec;  // HTTP request timeout in seconds
    std::string notify_email; // email to alert on failure
    int         priority;     // 0=normal, 1=high
    std::string scheduled_at; // ISO8601 timestamp when this job was scheduled

    Task() : id(0), website_id(0), timeout_sec(30), priority(0) {}

    Task(int id, int website_id, const std::string& url,
         const std::string& keyword, int timeout_sec,
         const std::string& notify_email)
        : id(id), website_id(website_id), url(url),
          keyword(keyword), timeout_sec(timeout_sec),
          notify_email(notify_email), priority(0) {}
};

} // namespace webnotifier
