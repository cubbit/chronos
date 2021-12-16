#include "chronos.hpp"

namespace cubbit
{
    chronos::chronos(std::map<int, int> configuration)
        : _configuration(configuration)
    {
        for(auto& [category, limit] : this->_configuration)
            this->_current_state[category] = 0;

        this->_jobs_thread = std::thread(
            [this]
            {
                this->_scheduler = std::make_unique<marl::Scheduler>(marl::Scheduler::Config::allCores());
                this->_scheduler->bind();
                defer(this->_scheduler->unbind());

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
            });
    }

    chronos::~chronos()
    {
        this->_pending_tasks.wait();

        this->shutdown();

        this->_scheduler = nullptr;

        if(this->_jobs_thread.joinable())
            this->_jobs_thread.join();
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

    bool chronos::can_schedule(int category)
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
} // namespace cubbit
