#include <iostream>
#include <thread>

#include "TaskQueue.h"

int main()
{
    TaskQueue queue;

    std::thread worker([&queue]()
    {
        Task task = queue.pop();

        std::cout << "Worker received task:\n";
        std::cout << "Website ID: " << task.websiteID << "\n";
        std::cout << "URL: " << task.url << "\n";
    });

    Task task;
    task.websiteID = 1;
    task.url = "https://google.com";

    std::cout << "Scheduler adding task...\n";

    queue.push(task);

    worker.join();

    return 0;
}