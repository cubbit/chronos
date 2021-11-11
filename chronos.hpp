#pragma once

#include <map>
#include <marl/defer.h>
#include <marl/scheduler.h>
#include <marl/waitgroup.h>
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
        marl::Scheduler _scheduler;
        std::map<int, int> _configuration;

        cubbit::mutex _mutex;
        cubbit::condition_variable _condition;

        std::map<int, std::atomic<unsigned int>> _current_state;
        bool _shutdown{false};

        marl::WaitGroup _pending_tasks;

    public:
        chronos(std::map<int, int> configuration)
            : _scheduler(marl::Scheduler::Config::allCores()),
              _configuration(configuration)
        {
            for(auto& [category, limit] : this->_configuration)
                this->_current_state[category] = 0;

            this->_scheduler.bind();
        }

        ~chronos()
        {
            this->_pending_tasks.wait();

            defer(this->_scheduler.unbind());
        }

        void shutdown()
        {
            cubbit::unique_lock<cubbit::mutex> lock(this->_mutex);
            this->_shutdown = true;
            this->_condition.notify_all();
        }

        void wait()
        {
            this->_pending_tasks.wait();
        }

        bool can_schedule(int category)
        {
            if(this->_configuration.find(category) == this->_configuration.end())
                return false;

            {
                cubbit::unique_lock<cubbit::mutex> lock(this->_mutex);
                this->_condition.wait(lock, [&]
                                      { return this->_shutdown || this->_current_state[category] < this->_configuration[category]; });

                if(this->_shutdown)
                    throw std::system_error(make_error_code(std::errc::operation_canceled), "Cannot schedule task");

                this->_current_state[category]++;
                this->_pending_tasks.add();
            }

            return true;
        }

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

            marl::schedule([&, category, task = std::move(std::forward<Callable>(task)), promise]() mutable
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

            marl::schedule([&, category, promise]() mutable
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
