#include "chronos.hpp"

namespace cubbit
{
    std::weak_ptr<chronos> chronos::_instance{};

    chronos::chronos(std::map<category_type, int> configuration)
        : _configuration(configuration)
    {
        for(auto& [category, limit] : this->_configuration)
            this->_current_state[category] = 0;

        this->_jobs_thread = std::thread(
            [this]
            {
                {
                    auto scheduler = std::make_unique<marl::Scheduler>(marl::Scheduler::Config::allCores());
                    scheduler->bind();
                    defer(scheduler->unbind());
                    this->_active = true;
                    chronos::_instance = this->weak_from_this();

                    while(true)
                    {
                        cubbit::unique_lock<cubbit::mutex> lock(this->_mutex);
                        this->_condition.wait(lock, [&]
                                              { return this->_shutdown || this->_job_queue.size() > 0; });

                        if(this->_shutdown)
                            break;

                        std::lock_guard<cubbit::mutex> lock_guard(this->_job_mutex);

                        auto& job = this->_job_queue.front();

                        marl::schedule(std::move(job));

                        this->_job_queue.pop();
                    }
                }

                this->_active = false;
                this->_condition.notify_all();
            });

        this->_jobs_thread.detach();
    }

    chronos::~chronos()
    {
        this->shutdown();

        this->_pending_tasks.wait();

        cubbit::unique_lock<cubbit::mutex> lock(this->_mutex);
        this->_condition.wait(lock, [this]
                              { return !this->_active; });
    }

    void chronos::shutdown()
    {
        cubbit::unique_lock<cubbit::mutex> lock(this->_mutex);
        this->_shutdown = true;
        this->_condition.notify_all();
    }

    void chronos::wait()
    {
        this->_pending_tasks.wait();
    }

    bool chronos::can_schedule(category_type category)
    {
        if(category == generic)
        {
            this->_pending_tasks.add();

            return true;
        }

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
} // namespace cubbit
