#pragma once

#include <memory>

#include "shared_state.hpp"

namespace cubbit
{
    template <typename T>
    class future
    {
        std::shared_ptr<shared_state<T>> _state;

    public:
        future() = default;
        future(std::shared_ptr<shared_state<T>> state) : _state(std::move(state)){};

        future(const T& value) : _state(std::make_shared<shared_state<T>>(value)) {}
        future(T&& value) : _state(std::make_shared<shared_state<T>>(std::move(value))) {}

        ~future() = default;

        void wait() const
        {
            this->_state->wait();
        }

        T& get()
        {
            return this->_state->get();
        }
    };

    template <>
    class future<void>
    {
        std::shared_ptr<shared_state<void>> _state;

    public:
        future() = default;
        future(std::shared_ptr<shared_state<void>> state) : _state(std::move(state)){};

        ~future() = default;

        void wait() const
        {
            this->_state->wait();
        }

        void get()
        {
            this->_state->get();
        }
    };
} // namespace cubbit
