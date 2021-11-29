#include "chronos.hpp"

namespace cubbit
{
    marl::Scheduler chronos::_scheduler = marl::Scheduler::Config::allCores();
}
