// ============================================================
// queue/include/monitoring_result.h
// MonitoringResult - contract between Worker and DB Writer
// Owner: Shivank Garg
// ============================================================
#pragma once
#include <string>

namespace webnotifier {

/**
 * @brief The result returned by a Worker after checking a website.
 * Written to the monitoring_results table by DatabaseWriter.
 */
struct MonitoringResult {
    int         website_id;      // FK to websites table
    int         job_id;          // FK to monitoring_jobs table
    std::string status;          // "UP", "DOWN", "TIMEOUT", "ERROR"
    int         http_code;       // e.g. 200, 404, 503; 0 if unreachable
    long        response_time_ms;// round-trip time in milliseconds
    bool        keyword_found;   // was the keyword present in response body?
    int         ssl_expiry_days; // days until SSL cert expires; -1 if N/A
    std::string error_message;   // human-readable error, if any
    std::string timestamp;       // ISO8601 timestamp of the check

    MonitoringResult()
        : website_id(0), job_id(0), http_code(0),
          response_time_ms(0), keyword_found(false),
          ssl_expiry_days(-1) {}
};

} // namespace webnotifier
