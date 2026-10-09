#include "db_writer.h"

#include <iostream>

using namespace webnotifier;

int main()
{
    DatabaseWriter db(
        "host=localhost dbname=webnotifier user=postgres password=webnotifier123"
    );

    if (!db.connect()) {
        std::cerr << "Database connection failed.\n";
        return 1;
    }

    MonitoringResult result;

    result.website_id = 1;
    result.job_id = 1;
    result.status = "UP";
    result.http_code = 200;
    result.response_time_ms = 250;
    result.keyword_found = true;
    result.ssl_expiry_days = 90;
    result.error_message = "";

    if (db.write_result(result)) {
        std::cout << "Result written successfully.\n";
    } else {
        std::cerr << "Failed to write result.\n";
        return 1;
    }

    db.disconnect();

    return 0;
}
