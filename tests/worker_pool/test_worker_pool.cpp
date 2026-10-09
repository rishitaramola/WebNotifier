#include "worker_pool.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace webnotifier;

int main()
{
    const char* db_connection = std::getenv("WEBNOTIFIER_DB_URL");

    if (db_connection == nullptr) {
        std::cerr << "WEBNOTIFIER_DB_URL is not set.\n";
        return 1;
    }

    TaskQueue queue(100);

    DatabaseWriter database_writer(db_connection);

    if (!database_writer.connect()) {
        std::cerr << "Database connection failed.\n";
        return 1;
    }

    WorkerPool pool(queue, database_writer, 4);

    std::cout << "Starting Worker Pool...\n";
    pool.start();

    queue.push(Task(
        1,
        1,
        "https://example.com",
        "",
        10,
        ""
    ));

    queue.push(Task(
        2,
        1,
        "https://www.google.com",
        "",
        10,
        ""
    ));

    queue.push(Task(
    3,
    1,
    "https://httpbin.org/status/404",
    "",
    10,
    ""
    ));
    
    queue.push(Task(
    5,
    1,
    "https://this-website-does-not-exist-12345.com",
    "",
    10,
    ""
));

    std::cout << "Tasks submitted.\n";

    std::this_thread::sleep_for(std::chrono::seconds(15));

    std::cout << "Stopping Worker Pool...\n";

    pool.stop();

    database_writer.disconnect();

    std::cout << "Worker Pool stopped.\n";

    return 0;
}
