// ============================================================
// worker_pool/include/network_checker.h
// Network Checker - Header
// Owner: Shivank Garg
// ============================================================

#pragma once

#include "task.h"
#include "monitoring_result.h"

namespace webnotifier {

/**
 * @brief Performs HTTP/HTTPS health checks for monitoring tasks.
 *
 * Uses libcurl to:
 *  - Perform HTTP GET requests
 *  - Capture HTTP response code
 *  - Measure response time
 *  - Check for an optional keyword
 *  - Detect timeout/network errors
 */
class NetworkChecker {
public:
    NetworkChecker() = default;
    ~NetworkChecker() = default;

    /**
     * @brief Check a website and generate a monitoring result.
     *
     * @param task Monitoring task received from TaskQueue.
     * @return MonitoringResult containing the check outcome.
     */
    MonitoringResult check(const Task& task);
};

} // namespace webnotifier
