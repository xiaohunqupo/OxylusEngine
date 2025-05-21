#include "TaskScheduler.hpp"

namespace ox {

auto TaskScheduler::init() -> std::expected<void, std::string> {
  OX_SCOPED_ZONE;
  task_scheduler = create_unique<enki::TaskScheduler>();
  task_scheduler->Initialize();
  task_sets.reserve(100);

  return {};
}

auto TaskScheduler::deinit() -> std::expected<void, std::string> {
  task_scheduler->WaitforAllAndShutdown();
  return {};
}

void TaskScheduler::wait_for_all() {
  task_scheduler->WaitforAll();

  task_sets.clear();
}
} // namespace ox
