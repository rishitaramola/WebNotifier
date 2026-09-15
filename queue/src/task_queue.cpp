// ============================================================
// queue/src/task_queue.cpp
// Thread-Safe Task Queue - Implementation
// Owner: Shivank Garg
// ============================================================
#include "../include/task_queue.h"
#include <stdexcept>

namespace webnotifier {

TaskQueue::TaskQueue(size_t max_capacity)
    : stopped_(false), max_capacity_(max_capacity) {}

void TaskQueue::push(const Task& task) {
    // TODO: Acquire mutex, wait if full, push task, notify one consumer
    // Implementation outline:
    //   std::unique_lock<std::mutex> lock(mutex_);
    //   not_full_cv_.wait(lock, [this]{ return queue_.size() < max_capacity_ || stopped_; });
    //   if (stopped_) return;
    //   queue_.push(task);
    //   not_empty_cv_.notify_one();

    std::unique_lock<std::mutex> lock(mutex_);
    not_full_cv_.wait(lock, [this] {
        return queue_.size() < max_capacity_ || stopped_.load();
    });
    if (stopped_.load()) return;
    queue_.push(task);
    not_empty_cv_.notify_one(); // Wake up one waiting worker
}

bool TaskQueue::pop(Task& task) {
    // TODO: Acquire mutex, wait until not empty, pop task
    std::unique_lock<std::mutex> lock(mutex_);
    not_empty_cv_.wait(lock, [this] {
        return !queue_.empty() || stopped_.load();
    });
    if (stopped_.load() && queue_.empty()) return false;
    task = queue_.front();
    queue_.pop();
    not_full_cv_.notify_one(); // Wake up a blocked producer
    return true;
}

bool TaskQueue::try_pop(Task& task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    task = queue_.front();
    queue_.pop();
    return true;
}

size_t TaskQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool TaskQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void TaskQueue::stop() {
    stopped_.store(true);
    not_empty_cv_.notify_all(); // Wake all blocked consumers
    not_full_cv_.notify_all();  // Wake all blocked producers
}

bool TaskQueue::is_stopped() const {
    return stopped_.load();
}

} // namespace webnotifier
