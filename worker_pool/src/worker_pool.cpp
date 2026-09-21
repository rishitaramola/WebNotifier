#include "worker_pool.h"

#include <iostream>
#include <stdexcept>

namespace webnotifier {

WorkerPool::WorkerPool(
    TaskQueue& task_queue,
    DatabaseWriter& database_writer,
    std::size_t worker_count
)
    : task_queue_(task_queue),
      database_writer_(database_writer),
      worker_count_(worker_count),
      running_(false)
{
    if (worker_count_ == 0) {
        throw std::invalid_argument("Worker count must be greater than 0");
    }
}

WorkerPool::~WorkerPool()
{
    stop();
}

void WorkerPool::start()
{
    if (running_.exchange(true)) {
        return;
    }

    workers_.reserve(worker_count_);

    try {
        for (std::size_t i = 0; i < worker_count_; ++i) {
            workers_.emplace_back(
                &WorkerPool::worker_loop,
                this,
                i + 1
            );
        }
    }
    catch (...) {
        running_ = false;
        task_queue_.stop();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();
        throw;
    }
}

void WorkerPool::stop()
{
    if (!running_.exchange(false)) {
        return;
    }

    task_queue_.stop();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
}

std::size_t WorkerPool::worker_count() const
{
    return worker_count_;
}

bool WorkerPool::is_running() const
{
    return running_.load();
}

void WorkerPool::worker_loop(std::size_t worker_id)
{
    while (true) {
        Task task;

        if (!task_queue_.pop(task)) {
            break;
        }

        try {
            MonitoringResult result = network_checker_.check(task);

            std::cout
                << "[Worker "
                << worker_id
                << "] Job "
                << task.id
                << " checked: "
                << result.status
                << " HTTP: "
                << result.http_code
                << '\n';

            if (!database_writer_.write_result(result)) {
                std::cerr
                    << "[Worker "
                    << worker_id
                    << "] Failed to save job "
                    << task.id
                    << '\n';
            }
        }
        catch (const std::exception& ex) {
            std::cerr
                << "[Worker "
                << worker_id
                << "] Task "
                << task.id
                << " failed: "
                << ex.what()
                << '\n';
        }
    }
}

} // namespace webnotifier