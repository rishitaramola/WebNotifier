// ============================================================
// worker_pool/include/db_writer.h
// Database Writer - Header
// Owner: Shivank Garg
// ============================================================

#pragma once

#include "monitoring_result.h"

#include <mutex>
#include <string>

#include <libpq-fe.h>

namespace webnotifier {

/**
 * @brief Writes MonitoringResult objects to PostgreSQL.
 *
 * The DatabaseWriter owns a PostgreSQL connection and serializes
 * database writes so multiple Worker threads can safely submit
 * monitoring results.
 */
class DatabaseWriter {
public:

    /**
     * @brief Construct a DatabaseWriter.
     *
     * @param connection_string PostgreSQL connection string.
     */
    explicit DatabaseWriter(const std::string& connection_string);

    /**
     * @brief Close the database connection.
     */
    ~DatabaseWriter();

    // Non-copyable.
    DatabaseWriter(const DatabaseWriter&) = delete;
    DatabaseWriter& operator=(const DatabaseWriter&) = delete;

    /**
     * @brief Connect to PostgreSQL.
     *
     * @return true if connection succeeds.
     */
    bool connect();

    /**
     * @brief Close the current PostgreSQL connection.
     */
    void disconnect();

    /**
     * @brief Write a monitoring result to the database.
     *
     * @param result Result produced by NetworkChecker.
     *
     * @return true if the result was successfully stored.
     */
    bool write_result(const MonitoringResult& result);

    /**
     * @brief Check whether the database connection is active.
     */
    bool is_connected() const;

private:

    std::string connection_string_;

    PGconn* connection_;

    mutable std::mutex mutex_;
};

} // namespace webnotifier
