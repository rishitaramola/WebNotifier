// ============================================================
// worker_pool/src/db_writer.cpp
// Database Writer - Implementation
// Owner: Shivank Garg
// ============================================================

#include "db_writer.h"

#include <libpq-fe.h>

#include <iostream>
#include <cstdlib>

namespace webnotifier {

DatabaseWriter::DatabaseWriter(const std::string& connection_string)
    : connection_string_(connection_string),
      connection_(nullptr)
{
}

DatabaseWriter::~DatabaseWriter()
{
    disconnect();
}

bool DatabaseWriter::connect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Already connected.
    if (connection_ != nullptr &&
        PQstatus(connection_) == CONNECTION_OK) {
        return true;
    }

    // Clean up a failed/old connection first.
    if (connection_ != nullptr) {
        PQfinish(connection_);
        connection_ = nullptr;
    }

    connection_ = PQconnectdb(connection_string_.c_str());

    if (connection_ == nullptr) {
        std::cerr << "[DatabaseWriter] Failed to allocate PostgreSQL connection\n";
        return false;
    }

    if (PQstatus(connection_) != CONNECTION_OK) {
        std::cerr
            << "[DatabaseWriter] PostgreSQL connection failed: "
            << PQerrorMessage(connection_);

        PQfinish(connection_);
        connection_ = nullptr;

        return false;
    }

    std::cout << "[DatabaseWriter] Connected to PostgreSQL\n";

    return true;
}

void DatabaseWriter::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ != nullptr) {
        PQfinish(connection_);
        connection_ = nullptr;

        std::cout << "[DatabaseWriter] Disconnected from PostgreSQL\n";
    }
}

bool DatabaseWriter::write_result(const MonitoringResult& result)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (connection_ == nullptr ||
        PQstatus(connection_) != CONNECTION_OK) {

        std::cerr
            << "[DatabaseWriter] Cannot write result: "
            << "database is not connected\n";

        return false;
    }

    /*
     * Parameterized INSERT.
     *
     * PostgreSQL will generate:
     *   id         -> SERIAL
     *   checked_at -> DEFAULT NOW()
     *
     * Therefore we only provide the actual monitoring values.
     */
    const char* query =
        "INSERT INTO monitoring_results "
        "(website_id, job_id, status, http_code, response_time_ms, "
        " keyword_found, ssl_expiry_days, error_message) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
        "RETURNING id;";

    const std::string website_id = std::to_string(result.website_id);
    const std::string job_id = std::to_string(result.job_id);
    const std::string http_code = std::to_string(result.http_code);
    const std::string response_time =
        std::to_string(result.response_time_ms);

    const std::string keyword_found =
        result.keyword_found ? "true" : "false";

    const std::string ssl_expiry_days =
        std::to_string(result.ssl_expiry_days);

    const char* values[8] = {
        website_id.c_str(),
        job_id.c_str(),
        result.status.c_str(),
        http_code.c_str(),
        response_time.c_str(),
        keyword_found.c_str(),
        ssl_expiry_days.c_str(),
        result.error_message.c_str()
    };

    PGresult* query_result = PQexecParams(
        connection_,
        query,
        8,
        nullptr,
        values,
        nullptr,
        nullptr,
        0
    );

    if (query_result == nullptr) {
        std::cerr
            << "[DatabaseWriter] Database query failed: "
            << PQerrorMessage(connection_);

        return false;
    }

    const ExecStatusType status = PQresultStatus(query_result);

    // With RETURNING id the successful status is PGRES_TUPLES_OK.
    if (status != PGRES_TUPLES_OK) {
        std::cerr
            << "[DatabaseWriter] INSERT failed: "
            << PQresultErrorMessage(query_result);

        PQclear(query_result);
        return false;
    }

    int result_id = -1;
    if (PQntuples(query_result) > 0) {
        result_id = std::atoi(PQgetvalue(query_result, 0, 0));
    }

    PQclear(query_result);

    // Generate any alerts triggered by this result. Failures here are
    // logged but do not fail the write of the result itself.
    if (result_id > 0) {
        create_alerts(result, result_id);
    }

    return true;
}

void DatabaseWriter::create_alerts(const MonitoringResult& result, int result_id)
{
    // NOTE: caller (write_result) already holds mutex_.

    const std::string website_id = std::to_string(result.website_id);
    const std::string result_id_str = std::to_string(result_id);

    // Helper to run a single parameterized statement.
    auto exec = [&](const char* sql, int n, const char* const* vals) {
        PGresult* r = PQexecParams(connection_, sql, n, nullptr,
                                   vals, nullptr, nullptr, 0);
        if (r == nullptr || PQresultStatus(r) != PGRES_COMMAND_OK) {
            std::cerr << "[DatabaseWriter] Alert insert failed: "
                      << (r ? PQresultErrorMessage(r)
                            : PQerrorMessage(connection_));
        }
        if (r) PQclear(r);
    };

    // --- DOWN / TIMEOUT: fire only on state transition ---
    // Suppress a new alert if the immediately previous result for this
    // website was already in the same failing status (avoids flooding
    // one alert per check while a site stays down).
    if (result.status == "DOWN" || result.status == "TIMEOUT") {
        const char* type = result.status.c_str();
        const std::string message =
            "Website entered " + result.status + " state"
            + (result.error_message.empty()
                   ? std::string()
                   : (": " + result.error_message));

        const char* vals[4] = {
            website_id.c_str(),
            result_id_str.c_str(),
            type,
            message.c_str()
        };

        // Insert only if the previous result's status differs from this one.
        // The scalar subquery is NULL when there is no earlier result, and
        // NULL IS DISTINCT FROM $3 is TRUE, so the first failure also alerts.
        // $3 is cast to varchar explicitly: in the INSERT..SELECT target list an
        // untyped parameter defaults to text, but the IS DISTINCT FROM comparison
        // against monitoring_results.status (varchar) forces varchar — without the
        // cast Postgres rejects the conflicting deductions for $3.
        const char* sql =
            "INSERT INTO alerts (website_id, result_id, alert_type, message) "
            "SELECT $1, $2, $3::varchar, $4 "
            "WHERE ("
            "  SELECT mr.status FROM monitoring_results mr "
            "  WHERE mr.website_id = $1 AND mr.id <> $2 "
            "  ORDER BY mr.id DESC LIMIT 1"
            ") IS DISTINCT FROM $3::varchar;";

        exec(sql, 4, vals);
    }

    // --- KEYWORD_MISSING: a keyword was expected but not found ---
    // (keyword_found defaults to true when no keyword is configured.)
    if (!result.keyword_found) {
        const std::string message = "Expected keyword not found in response";
        const char* vals[3] = {
            website_id.c_str(),
            result_id_str.c_str(),
            message.c_str()
        };
        const char* sql =
            "INSERT INTO alerts (website_id, result_id, alert_type, message) "
            "VALUES ($1, $2, 'KEYWORD_MISSING', $3);";
        exec(sql, 3, vals);
    }

    // --- SSL_EXPIRY: cert expiring within the site's threshold ---
    // ssl_expiry_days == -1 means "not applicable / not measured".
    if (result.ssl_expiry_days >= 0) {
        const std::string days = std::to_string(result.ssl_expiry_days);
        const std::string message =
            "SSL certificate expires in " + days + " day(s)";
        const char* vals[4] = {
            website_id.c_str(),
            result_id_str.c_str(),
            days.c_str(),
            message.c_str()
        };
        // Compare against the per-site threshold; only one open (unacknowledged)
        // SSL alert per site at a time.
        const char* sql =
            "INSERT INTO alerts (website_id, result_id, alert_type, message) "
            "SELECT $1, $2, 'SSL_EXPIRY', $4 FROM websites w "
            "WHERE w.id = $1 AND $3::int <= w.ssl_alert_days "
            "AND NOT EXISTS ("
            "  SELECT 1 FROM alerts a "
            "  WHERE a.website_id = $1 AND a.alert_type = 'SSL_EXPIRY' "
            "    AND a.acknowledged = FALSE"
            ");";
        exec(sql, 4, vals);
    }
}

bool DatabaseWriter::is_connected() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return connection_ != nullptr &&
           PQstatus(connection_) == CONNECTION_OK;
}

} // namespace webnotifier
