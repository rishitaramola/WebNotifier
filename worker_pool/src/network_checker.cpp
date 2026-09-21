// ============================================================
// worker_pool/src/network_checker.cpp
// Network Checker - Implementation
// Owner: Shivank Garg
// ============================================================

#include "network_checker.h"

#include <curl/curl.h>

#include <chrono>
#include <sstream>
#include <string>
#include <iomanip>
namespace webnotifier {

namespace {

/**
 * @brief libcurl callback used to collect the HTTP response body.
 */
size_t write_callback(void* contents, size_t size, size_t nmemb, void* user_data)
{
    const size_t total_size = size * nmemb;

    auto* response_body = static_cast<std::string*>(user_data);
    response_body->append(static_cast<char*>(contents), total_size);

    return total_size;
}

} // namespace

MonitoringResult NetworkChecker::check(const Task& task)
{
    MonitoringResult result;

    result.website_id = task.website_id;
    result.job_id = task.id;

    // Get current timestamp.
    const auto now = std::chrono::system_clock::now();
    const auto now_time = std::chrono::system_clock::to_time_t(now);

    std::tm utc_time{};

#ifdef _WIN32
    gmtime_s(&utc_time, &now_time);
#else
    gmtime_r(&now_time, &utc_time);
#endif

    std::ostringstream timestamp_stream;
    timestamp_stream << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    result.timestamp = timestamp_stream.str();

    CURL* curl = curl_easy_init();

    if (!curl) {
        result.status = "ERROR";
        result.error_message = "Failed to initialize libcurl";
        return result;
    }

    std::string response_body;

    curl_easy_setopt(curl, CURLOPT_URL, task.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

    // Follow HTTP redirects.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Timeout configured by the Task.
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, task.timeout_sec);

    // Connection timeout.
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, task.timeout_sec);

    // Identify our monitoring client.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WebNotifier/1.0");

    // Do not print anything to stdout/stderr.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const auto start_time = std::chrono::steady_clock::now();

    const CURLcode curl_result = curl_easy_perform(curl);

    const auto end_time = std::chrono::steady_clock::now();

    result.response_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();

    if (curl_result == CURLE_OK) {

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        result.http_code = static_cast<int>(http_code);

        // HTTP 2xx and 3xx are considered reachable.
        if (http_code >= 200 && http_code < 400) {
            result.status = "UP";
        } else {
            result.status = "DOWN";
        }

        // Check optional keyword.
        if (!task.keyword.empty()) {
            result.keyword_found =
                response_body.find(task.keyword) != std::string::npos;
        } else {
            result.keyword_found = true;
        }

    } else if (curl_result == CURLE_OPERATION_TIMEDOUT) {

        result.status = "TIMEOUT";
        result.error_message = "HTTP request timed out";

    } else {

        result.status = "ERROR";
        result.error_message = curl_easy_strerror(curl_result);
    }

    curl_easy_cleanup(curl);

    return result;
}

} // namespace webnotifier
