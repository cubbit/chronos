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
    using category_type = int;
    static constexpr category_type generic = std::numeric_limits<category_type>::max();

    template <category_type Category>
    struct task
    {
        category_type category() const { return Category; }
    };

    //forward declaration
    template <typename Callable>
    future<typename std::result_of<Callable()>::type>
    async(Callable&& callable, category_type category = generic);

    class chronos : public std::enable_shared_from_this<chronos>
    {
        static std::weak_ptr<chronos> _instance;

        std::unique_ptr<marl::Scheduler> _scheduler;
        std::map<category_type, int> _configuration;
        std::queue<std::function<void()>> _job_queue;
        std::thread _jobs_thread;
        cubbit::mutex _job_mutex;
        cubbit::mutex _mutex;
        cubbit::condition_variable _condition;
        std::map<category_type, std::atomic<unsigned int>> _current_state;
        bool _active{false};
        bool _shutdown{false};
        marl::WaitGroup _pending_tasks;

        chronos();
        chronos(std::map<category_type, int> configuration);

    public:
        ~chronos();

        void shutdown();
        void wait();
        bool can_schedule(category_type category);

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
        schedule(Callable&& task, category_type category)
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

                    std::lock_guard<cubbit::mutex> lock(this->_mutex);
                    this->_current_state[category]--;
                    this->_condition.notify_all();
                });

            this->_condition.notify_all();

            return future;
        }

        template <typename Callable>
        future<typename std::result_of<Callable()>::type>
        schedule(Callable& task, category_type category)
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

                    std::lock_guard<cubbit::mutex> lock(this->_mutex);
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

        template <typename Callable>
        friend future<typename std::result_of<Callable()>::type>
        async(Callable&& callable, category_type category)
        {
            if(auto instance = chronos::_instance.lock(); instance)
                return instance->schedule(std::forward<Callable>(callable), category);

            throw std::runtime_error("Chronos is not initialized");
        }

        template <typename... Args>
        static std::shared_ptr<chronos> create(Args&&... args)
        {
            auto shared = std::shared_ptr<chronos>(new chronos(std::forward<Args>(args)...));
            chronos::_instance = shared->shared_from_this();

            return shared;
        }

    private:
        void _start_jobs_thread();
    };

    namespace this_fiber
    {
        template <typename Clock, typename Duration>
        static inline void sleep_until(const std::chrono::time_point<Clock, Duration>& timeout)
        {
            cubbit::mutex mutex;
            cubbit::condition_variable condition;

            cubbit::unique_lock<cubbit::mutex> lock(mutex);

            condition.wait_until(lock, timeout, []
                                 { return false; });
        }

        template <typename Clock, typename Duration>
        static inline void sleep_until(cubbit::unique_lock<cubbit::mutex>& lock, const std::chrono::time_point<Clock, Duration>& timeout)
        {
            cubbit::condition_variable condition;

            condition.wait_until(lock, timeout, []
                                 { return false; });
        }

        template <typename Rep, typename Period>
        static inline void sleep_for(const std::chrono::duration<Rep, Period>& duration)
        {
            const auto timeout = std::chrono::system_clock::now() + duration;
            cubbit::mutex mutex;
            cubbit::condition_variable condition;

            cubbit::unique_lock<cubbit::mutex> lock(mutex);

            condition.wait_until(lock, timeout, []
                                 { return false; });
        }
    } // namespace this_fiber
} // namespace cubbit
