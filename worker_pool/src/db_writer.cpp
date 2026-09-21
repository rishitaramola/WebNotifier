// ============================================================
// worker_pool/src/db_writer.cpp
// Database Writer - Implementation
// Owner: Shivank Garg
// ============================================================

#include "db_writer.h"

#include <libpq-fe.h>

#include <iostream>

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
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8);";

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

    if (status != PGRES_COMMAND_OK) {
        std::cerr
            << "[DatabaseWriter] INSERT failed: "
            << PQresultErrorMessage(query_result);

        PQclear(query_result);
        return false;
    }

    PQclear(query_result);

    return true;
}

bool DatabaseWriter::is_connected() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return connection_ != nullptr &&
           PQstatus(connection_) == CONNECTION_OK;
}

} // namespace webnotifier
