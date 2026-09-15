// ============================================================
// queue/include/task_queue.h
// Thread-Safe Task Queue - Header
// Owner: Shivank Garg
// OS Concepts: Mutex, Condition Variable, Producer-Consumer
// ============================================================
#pragma once
#include "task.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace webnotifier {

/**
 * @brief A thread-safe blocking FIFO queue for Task objects.
 *
 * OS Concepts Demonstrated:
 *  - std::mutex          -> critical section protection
 *  - std::condition_variable -> blocks consumer threads when empty
 *  - Producer-Consumer pattern -> Scheduler pushes, Workers pop
 */
class TaskQueue {
public:
    explicit TaskQueue(size_t max_capacity = 1000);
    ~TaskQueue() = default;

    // Non-copyable, non-movable
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    /**
     * @brief Push a task onto the queue (Producer).
     * Blocks if queue is at max capacity.
     * @param task The task to enqueue.
     */
    void push(const Task& task);

    /**
     * @brief Pop a task from the queue (Consumer).
     * Blocks until a task is available or the queue is stopped.
     * @param task Output parameter for the retrieved task.
     * @return false if the queue has been stopped and is empty.
     */
    bool pop(Task& task);

    /**
     * @brief Try to pop without blocking.
     * @return false if no task is available immediately.
     */
    bool try_pop(Task& task);

    /** @brief Returns current number of tasks in the queue. */
    size_t size() const;

    /** @brief Returns true if queue is empty. */
    bool empty() const;

    /**
     * @brief Signal all waiting consumers to unblock and stop.
     * Called during graceful shutdown.
     */
    void stop();

    /** @brief Returns true if stop() has been called. */
    bool is_stopped() const;

private:
    std::queue<Task>        queue_;
    mutable std::mutex      mutex_;          // OS: Mutex for critical section
    std::condition_variable not_empty_cv_;  // OS: Consumer waits here
    std::condition_variable not_full_cv_;   // OS: Producer waits here (backpressure)
    std::atomic<bool>       stopped_;
    size_t                  max_capacity_;
};

} // namespace webnotifier
