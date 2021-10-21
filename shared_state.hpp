#pragma once
#include "condition_variable.hpp"

#include <future>
#include <marl/waitgroup.h>

namespace cubbit
{
    class shared_state_base
    {
    protected:
        marl::WaitGroup _wg{1};
        cubbit::mutex _mutex;
        std::exception_ptr _exception;
        bool _done{false};

        void _mark_done_and_notify()
        {
            this->_done = true;
            this->_wg.done();
        }

    public:
        virtual ~shared_state_base()
        {
        }

        void set_exception(std::exception_ptr exception)
        {
            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            if(this->_done)
                throw std::future_error(std::future_errc::promise_already_satisfied);

            this->_exception = exception;
            this->_mark_done_and_notify();
        }

        void wait()
        {
            this->_wg.wait();
        }
    };

    template <typename T>
    class shared_state : public shared_state_base
    {
        std::unique_ptr<T> _value;

    public:
        ~shared_state()
        {
        }

        void set_value(const T& value)
        {
            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            if(this->_done)
                throw std::future_error(std::future_errc::promise_already_satisfied);

            this->_value = std::make_unique<T>(value);
            this->_mark_done_and_notify();
        }

        void set_value(T&& value)
        {
            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            if(this->_done)
                throw std::future_error(std::future_errc::promise_already_satisfied);

            this->_value = std::make_unique<T>(std::move(value));
            this->_mark_done_and_notify();
        }

        T& get()
        {
            this->_wg.wait();

            if(this->_exception)
                std::rethrow_exception(this->_exception);

            return *this->_value;
        }
    };

    template <>
    class shared_state<void> : public shared_state_base
    {
    public:
        ~shared_state()
        {
        }

        void set_value()
        {
            std::lock_guard<cubbit::mutex> lock(this->_mutex);

            if(this->_done)
                throw std::future_error(std::future_errc::promise_already_satisfied);

            this->_mark_done_and_notify();
        }

        void get()
        {
            this->_wg.wait();

            if(this->_exception)
                std::rethrow_exception(this->_exception);
        }
    };
} // namespace cubbit
