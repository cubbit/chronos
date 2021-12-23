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

struct not_def_constr
{
    int value;

    not_def_constr() = delete;
    not_def_constr(int val) : value(val) {}
};

struct def_constr
{
    int value;
};

int main()
{
    cubbit::mutex mutex;
    cubbit::condition_variable condition;

    auto chronos_scheduler = cubbit::chronos::create(std::map<int, int>{{future_task, 6}, {cpu_task, 12}});

    for(int i = 0; i < 30; ++i)
    {
        chronos_scheduler->schedule([i]
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

        futures.push_back(chronos_scheduler->schedule(std::move(lambda), future_task));
    }

    auto test_sleep_for = cubbit::async(
        []
        {
            auto start = std::chrono::system_clock::now();
            cubbit::this_fiber::sleep_for(std::chrono::seconds(2));
            std::chrono::duration<double> diff = std::chrono::system_clock::now() - start;
            std::cout << "sleep_for: slept for " << diff.count() << std::endl;

            return not_def_constr{42};
        });

    auto test_sleep_until = cubbit::async(
        []
        {
            auto start = std::chrono::system_clock::now();
            auto until = start + std::chrono::seconds(2);
            cubbit::this_fiber::sleep_until(until);
            std::chrono::duration<double> diff = std::chrono::system_clock::now() - start;
            std::cout << "sleep_until: slept for " << diff.count() << std::endl;

            return def_constr{43};
        });

    auto test_immediate_future1 = cubbit::future<not_def_constr>(not_def_constr{44});
    auto test_immediate_future2 = cubbit::future<def_constr>(def_constr{45});

    for(int i = 0; i < task_num; ++i)
        std::cout << "task finished: " << futures[i].get() << std::endl;

    handle handle;
    chronos_scheduler->schedule(std::move(handle)).get();

    std::cout << test_sleep_for.get().value << std::endl;
    std::cout << test_sleep_until.get().value << std::endl;
    std::cout << test_immediate_future1.get().value << std::endl;
    std::cout << test_immediate_future2.get().value << std::endl;
}
