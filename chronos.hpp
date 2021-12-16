#pragma once

#include <iostream>
#include <map>
#include <marl/defer.h>
#include <marl/scheduler.h>
#include <marl/waitgroup.h>
#include <queue>
#include <string>
#include <type_traits>

#include "condition_variable.hpp"
#include "promise.hpp"

namespace cubbit
{
    template <int Category>
    struct task
    {
        int category() const { return Category; }
    };

    class chronos
    {
        std::unique_ptr<marl::Scheduler> _scheduler;
        std::map<int, int> _configuration;
        std::queue<std::function<void()>> _job_queue;
        std::thread _jobs_thread;
        cubbit::mutex _job_mutex;
        cubbit::mutex _mutex;
        cubbit::condition_variable _condition;
        std::map<int, std::atomic<unsigned int>> _current_state;
        bool _shutdown{false};
        marl::WaitGroup _pending_tasks;

    public:
        chronos(std::map<int, int> configuration);

        ~chronos();

        void shutdown();

        void wait();

        bool can_schedule(int category);

        template <typename Callable>
        void execute_task(Callable& task, promise<typename std::enable_if<!std::is_void<typename std::result_of<Callable()>::type>::value, typename std::result_of<Callable()>::type>::type>& _promise)
        {
            _promise.set_value(task());
        }

        template <typename Callable>
        void execute_task(Callable& task, promise<typename std::enable_if<std::is_void<typename std::result_of<Callable()>::type>::value, typename std::result_of<Callable()>::type>::type>& _promise)
        {
            task();
            _promise.set_value();
        }

        template <typename Callable>
        future<typename std::result_of<Callable()>::type>
        schedule(Callable&& task, int category)
        {
            if(!this->can_schedule(category))
                throw std::system_error(make_error_code(std::errc::invalid_argument));

            promise<typename std::result_of<Callable()>::type> promise;
            auto future = promise.get_future();

            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            this->_job_queue.emplace(
                [&, category, task = std::move(std::forward<Callable>(task)), promise]() mutable
                {
                    defer(this->_pending_tasks.done());

                    try
                    {
                        execute_task(task, promise);
                    }
                    catch(std::exception& exception)
                    {
                        promise.set_exception(make_exception_ptr(exception));
                    }

                    std::lock_guard lock(this->_mutex);
                    this->_current_state[category]--;
                    this->_condition.notify_all();
                });

            this->_condition.notify_all();

            return future;
        }

        template <typename Callable>
        future<typename std::result_of<Callable()>::type>
        schedule(Callable& task, int category)
        {
            if(!this->can_schedule(category))
                throw std::system_error(make_error_code(std::errc::invalid_argument));

            promise<typename std::result_of<Callable()>::type> promise;
            auto future = promise.get_future();

            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            this->_job_queue.emplace(
                [this, &task, category, promise]() mutable
                {
                    defer(this->_pending_tasks.done());

                    try
                    {
                        execute_task(task, promise);
                    }
                    catch(std::exception& exception)
                    {
                        promise.set_exception(make_exception_ptr(exception));
                    }

                    std::lock_guard lock(this->_mutex);
                    this->_current_state[category]--;
                    this->_condition.notify_all();
                });

            this->_condition.notify_all();

            return future;
        }

        template <typename Task>
        future<typename std::result_of<Task()>::type>
        schedule(Task&& task)
        {
            return this->schedule(std::forward<Task>(task), task.category());
        }
    };
} // namespace cubbit
