#include "../chronos.hpp"
#include "marl/defer.h"
#include "marl/event.h"
#include "marl/scheduler.h"
#include "marl/waitgroup.h"

#include <iostream>
#include <memory>
#include <vector>

enum category
{
    future_task,
    cpu_task,
};

struct handle : public cubbit::task<future_task>
{
    void operator()() const
    {
        this->_run();
    }

    void _run() const
    {
        std::cout << "run handle" << std::endl;
    }
};

int main()
{
    cubbit::mutex mutex;
    cubbit::condition_variable condition;

    cubbit::chronos chronos_scheduler({{future_task, 6}, {cpu_task, 12}});

    for(int i = 0; i < 30; ++i)
    {
        chronos_scheduler.schedule([i]
                                   {
                                       volatile size_t sum = 0;

                                       for(int j = 0; j < 1000000000; ++j)
                                           sum += j * i;

                                       std::cout << "sum: " << sum << std::endl;
                                   },
                                   cpu_task);
    }

    int task_num = 6;
    std::vector<cubbit::future<int>> futures;

    for(int i = 0; i < task_num; ++i)
    {
        std::cout << "scheduling " << i << std::endl;

        auto lambda = [&condition, &mutex, i]() -> int
        {
            std::cout << i << ": started" << std::endl;

            cubbit::unique_lock<cubbit::mutex> lock(mutex);
            condition.wait_for(lock, std::chrono::seconds(2), []
                               { return false; });

            std::cout << i << ": finished" << std::endl;
            return i;
        };

        futures.push_back(chronos_scheduler.schedule(lambda, future_task));
    }

    for(int i = 0; i < task_num; ++i)
        std::cout << "task finished: " << futures[i].get() << std::endl;

    handle handle;
    chronos_scheduler.schedule(std::move(handle)).get();
}
