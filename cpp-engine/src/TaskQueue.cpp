#include "TaskQueue.h"

void TaskQueue::push(Task task)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(task);
    }

    cv.notify_one();
}

Task TaskQueue::pop()
{
    std::unique_lock<std::mutex> lock(mtx);

    cv.wait(lock, [this]() {
        return !tasks.empty();
    });

    Task task = tasks.front();
    tasks.pop();

    return task;
}

bool TaskQueue::empty()
{
    std::lock_guard<std::mutex> lock(mtx);
    return tasks.empty();
}