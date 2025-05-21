#pragma once
#include <enkiTS/TaskScheduler.h>

#include "Core/ESystem.hpp"

namespace ox {
using TaskSet = enki::TaskSet;
using ITaskSet = enki::ITaskSet;
using IPinnedTask = enki::IPinnedTask;
using ICompleteableTask = enki::ICompletable;
using AsyncTask = enki::TaskSet;
using TaskSetPartition = enki::TaskSetPartition;
using TaskFunction = enki::TaskSetFunction;

class TaskScheduler : public ESystem {
public:
  TaskScheduler() = default;

  auto init() -> std::expected<void, std::string> override;
  auto deinit() -> std::expected<void, std::string> override;

  Unique<enki::TaskScheduler>& get_underlying() { return task_scheduler; }

  void schedule_task(ITaskSet* set) const { task_scheduler->AddTaskSetToPipe(set); }

  void schedule_task(IPinnedTask* set) const { task_scheduler->AddPinnedTask(set); }

  void wait_task(const ITaskSet* set) const { task_scheduler->WaitforTask(set); }

  void wait_for_all();

private:
  Unique<enki::TaskScheduler> task_scheduler;
  std::vector<Unique<TaskSet>> task_sets = {};
};
} // namespace ox
