#pragma once

#include "future.hpp"
#include "shared_state.hpp"

namespace cubbit
{
    template <typename T>
    class promise
    {
        std::shared_ptr<shared_state<T>> _state = std::make_shared<shared_state<T>>();
        bool _obtained{false};

    public:
        ~promise()
        {
        }

        future<T> get_future()
        {
            if(this->_obtained)
                throw std::future_error(std::future_errc::future_already_retrieved);

            this->_obtained = true;

            return future{_state};
        }

        void set_value(const T& value)
        {
            this->_state->set_value(value);
        }

        void set_value(T&& value)
        {
            this->_state->set_value(std::move(value));
        }

        void set_exception(std::exception_ptr exception)
        {
            this->_state->set_exception(exception);
        }
    };

    template <>
    class promise<void>
    {
        std::shared_ptr<shared_state<void>> _state = std::make_shared<shared_state<void>>();
        bool _obtained{false};

    public:
        ~promise()
        {
        }

        future<void> get_future()
        {
            if(this->_obtained)
                throw std::future_error(std::future_errc::future_already_retrieved);

            this->_obtained = true;

            return future{_state};
        }

        void set_value()
        {
            this->_state->set_value();
        }

        void set_exception(std::exception_ptr exception)
        {
            this->_state->set_exception(exception);
        }
    };
} // namespace cubbit
