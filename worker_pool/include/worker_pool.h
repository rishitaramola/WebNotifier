#pragma once

#include "task_queue.h"
#include "network_checker.h"
#include "db_writer.h"

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace webnotifier {

class WorkerPool {
public:
    WorkerPool(
        TaskQueue& task_queue,
        DatabaseWriter& database_writer,
        std::size_t worker_count
    );

    ~WorkerPool();

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void start();
    void stop();

    std::size_t worker_count() const;
    bool is_running() const;

private:
    void worker_loop(std::size_t worker_id);

    TaskQueue& task_queue_;
    DatabaseWriter& database_writer_;

    std::size_t worker_count_;
    std::vector<std::thread> workers_;

    std::atomic<bool> running_;

    NetworkChecker network_checker_;
};

}