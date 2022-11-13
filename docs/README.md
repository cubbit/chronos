---
id: cpp-chronos
title: Chronos scheduler
sidebar_label: Chronos
slug: /sdk/cpp/chronos
---

Chronos is a task/fiber scheduler based on [marl](https://github.com/google/marl).

## Synchronization

Chronos uses `mutex`, `lock` and `conditionVariable` from marl.
They are aliased for convenience in `cubbit` namespace.
Their interface is the same as the `std` ones.

If you use the `std` primitives in a fiber, the OS thread that is executing the
fiber blocks. Instead, you should use the `cubbit` primitives.
In this case the thread will run another fiber.
If you use a `cubbit` primitive in a thread that is not within the scheduler
thread pool, the library will fall back to the normal `std` primitive.

Example

```c++
#include <chronos/condition_variable.hpp>

cubbit::mutex mutex;
cubbit::condition_variable condition;
bool ready{false};

void main_fiber()
{
    cubbit::unique_lock<cubbit::mutex> lock;

    condition.wait(lock, [&ready]{return ready;});

    std::cout << "READY!" << std::endl;
}

void signal_fiber()
{
    {
        cubbit::unique_lock<cubbit::mutex> lock;
        ready = true;
    }

    condition.notify_all();
}
```

## Promises and Futures

Chronos implements `cubbit::promise` and `cubbit::future` and they have the same
interface as `std::promise` and `std::future`.
They use chronos synchronization primitives, so if you wait for a future within
a fiber, the underlying OS thread will switch to another fiber.

To use `cubbit::promise` and `cubbit::future` you need `#include <chronos/promise.hpp>`

## Utilities

Chronos provides utilities functions that mimic those in `std::this_thread`.
These functions allow a fiber to sleep, without blocking the thread pool.

```c++
template <typename Clock, typename Duration>
static inline void sleep_until(const std::chrono::time_point<Clock, Duration>& timeout)

// this function takes a unique_lock, which must be locked and owned. It will be
// unlocked when the sleep starts, and relocked before returning to the caller
template <typename Clock, typename Duration>
static inline void sleep_until(cubbit::unique_lock<cubbit::mutex>& lock, const std::chrono::time_point<Clock, Duration>& timeout)

template <typename Rep, typename Period>
static inline void sleep_for(const std::chrono::duration<Rep, Period>& duration)
```

## Initialization

Since marl uses static variables, there can only one instance of chronos.
Starting two chronos instances is undefined behaviour.

To create a instance use the `cubbit::chronos::create()` factory function.

### Categories

You can use Categories to limit the number of concurrent fibers of the same type.
The category configuration must be provided at construction and cannot be changed.

Then you can pass the category to the `schedule(Callable, Category)` function
to specify the concurrency policy.
If the concurrency limit for the specified category has been reached, the `schedule()`
function blocks until a slot has been freed.

The generic category `cubbit::generic` is always unlimited.

The `schedule()` function returns a future, which is resolved when the scheduled
fiber ends.

Example:

```cpp
enum category
{
    cat1,
    cat2,
    cat3,
};

std::map<int, int> configuration{
    {cat1, 1},
    {cat2, 2},
    {cat3, 3},
};

auto chronos = cubbit::chronos::create(configuration);

auto future1 = chronos->schedule([](){some_long_running_function();}, cat1);

// blocks until the first scheduled fiber has finished since cat1 limit is 1.
auto future2 = chronos->schedule([](){some_long_running_function();}, cat1);

chronos->schedule([](){some_long_running_function();}, cubbit::generic); // never blocks

future1.wait();
future2.wait();

// does not block because the previous fibers have ended.
chronos->schedule([](){some_long_running_function();}, cat1);
```

#### Tasks

Users can extend the `cubbit::task` struct to create a callable object with an
embedded category

Example:

```c++
enum category
{
    cat1,
    cat2,
    cat3,
};

std::map<int, int> configuration{
    {cat1, 1},
    {cat2, 2},
    {cat3, 3},
};

struct some_task : cubbit::task<cat2>
{
    int operator()() const
    {
        return some_long_running_function();
    }
}

auto chronos = cubbit::chronos::create(configuration);

auto future = chronos->schedule(some_task{}); // embedded cat2

int result = future.get();
```

## Termination

`cubbit::chronos::wait()` waits until there are no more fibers running on the
thread pool

`cubbit::chronos::shutdown()` prevents the scheduler to accept any new fiber,
while it lets already scheduled fibers to finish normally.

The destructor `cubbit::chronos::~chronos()` uses `shutdown()` and `wait()`,
which means that it waits for the running fibers to finish.

## Internals

The scheduler spawns a thread pool with a number of threads equals to the number
of available hardware cores.

When a fiber is scheduled, it is added to a queue which is processed by a
dedicated special thread of the thread pool.
This is needed because marl does not allow to schedule fibers from outside the
thread pool.

## Best practices

- Always use `chronos` primitives (`cubbit::mutex`, `cubbit::condition_variable`,
ecc...) inside fibers, because `std` primitives will block the thread pool
