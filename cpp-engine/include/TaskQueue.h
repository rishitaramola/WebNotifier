#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include<queue>
#include<mutex>
#include<condition_variable>

#include "Task.h"

class TaskQueue
{
private:
    std::queue<Task> tasks;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(Task task);
    Task pop();
    bool empty();
};

#endif
