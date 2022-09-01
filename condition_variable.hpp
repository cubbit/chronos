#pragma once
#include <marl/conditionvariable.h>
#include <marl/mutex.h>

namespace cubbit
{
    using mutex = marl::mutex;

    using condition_variable = marl::ConditionVariable;

    template <class T>
    using unique_lock = marl::lock;
} // namespace cubbit
