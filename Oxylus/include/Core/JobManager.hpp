#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <shared_mutex>
#include <thread>

#include "Core/Arc.hpp"
#include "Core/ESystem.hpp"

namespace ox {
using JobFn = std::function<void()>;

struct Job;
struct Barrier : ManagedObj {
  u32 acquired = 0;
  std::atomic<u32> counter = 0;
  std::vector<Arc<Job>> pending = {};

  static auto create() -> Arc<Barrier>;
  auto wait(this Barrier& self) -> void;
  auto acquire(this Barrier& self, u32 count = 1) -> Arc<Barrier>;
  auto add(this Barrier& self, Arc<Job> job) -> Arc<Barrier>;
};

class JobManager;
struct Job : ManagedObj {
  std::vector<Arc<Barrier>> barriers = {};
  JobFn task = {};

  static auto create_explicit(JobFn task) -> Arc<Job>;
  template <typename Fn>
  static auto create(Fn task) -> Arc<Job> {
    return create_explicit(std::forward<Fn>(task));
  }

  auto signal(this Job& self, Arc<Barrier> barrier) -> Arc<Job>;
};

struct ThreadWorker {
  u32 id = ~0_u32;
};

inline thread_local ThreadWorker this_thread_worker;

class JobManager : public ESystem {
public:
  static constexpr u32 auto_thread_count = 0;
  JobManager(u32 threads = auto_thread_count);
  ~JobManager() = default;

  auto init() -> std::expected<void, std::string> override;
  auto deinit() -> std::expected<void, std::string> override;

  auto shutdown(this JobManager& self) -> void;
  auto worker(this JobManager& self, u32 id) -> void;
  auto submit(this JobManager& self, Arc<Job> job, bool prioritize = false) -> void;
  auto wait(this JobManager& self) -> void;

  template <std::input_iterator Iterator, typename Func>
  auto for_each(this JobManager& self, Iterator begin, Iterator end, Func&& func) -> void {
    const usize total_size = std::distance(begin, end);
    if (total_size == 0)
      return;

    const usize chunk_size = std::max<usize>(1, total_size / (self.num_threads * 4));
    usize global_index = 0;

    for (auto chunk_start = begin; chunk_start != end;) {
      auto chunk_end = chunk_start;
      const usize chunk_start_index = global_index;
      std::advance(chunk_end, std::min(chunk_size, static_cast<usize>(std::distance(chunk_start, end))));
      global_index += std::distance(chunk_start, chunk_end);

      self.submit(Job::create([=, func = std::forward_like<Func>(func)] {
        usize local_index = chunk_start_index;
        for (auto it = chunk_start; it != chunk_end; ++it, ++local_index) {
          std::invoke(func, *it, local_index);
        }
      }));

      chunk_start = chunk_end;
    }
  }

  template <std::ranges::contiguous_range Range, typename Func>
  auto for_each(this JobManager& self, Range&& range, Func&& func) -> void {
    auto view = std::span(std::forward<Range>(range));
    self.for_each(view.begin(), view.end(), std::forward<Func>(func));
  }

  template <typename T>
  struct AsyncVectorHolder : ManagedObj {
    T data;
    std::atomic<u32> active_jobs{0};

    explicit AsyncVectorHolder(const T& src) : data(src) {}
    explicit AsyncVectorHolder(T&& src) : data(std::move(src)) {}
  };

  struct AsyncCompletionToken : ManagedObj {
    std::function<void()> callback;
    std::atomic<u32> pending_jobs{0};

    static auto create() -> Arc<AsyncCompletionToken> { return Arc<AsyncCompletionToken>::create(); }
  };

  template <std::ranges::contiguous_range T, typename Func>
  void for_each_async(this JobManager& self, T& vec, Func&& func, std::function<void()>&& completion_callback = {}) {
    auto token = AsyncCompletionToken::create();
    token->callback = completion_callback;
    auto holder = Arc<AsyncVectorHolder<T>>::create(vec);

    const size_t chunk_size = std::max<size_t>(1, vec.size() / (self.num_threads * 4));

    for (size_t start = 0; start < vec.size(); start += chunk_size) {
      const size_t end = std::min(start + chunk_size, vec.size());

      holder->acquire_ref();
      holder->active_jobs.fetch_add(1, std::memory_order_relaxed);
      token->pending_jobs.fetch_add(1, std::memory_order_relaxed);

      self.submit(Job::create([&self, token, holder, start, end, func = std::forward<Func>(func)]() {
        for (size_t i = start; i < end; ++i) {
          func(holder->data[i], i);
        }

        if (holder->active_jobs.fetch_sub(1, std::memory_order_release) == 1) {
          holder->release_ref();
          if (token->callback)
            self.submit(Job::create([t = std::move(token)]() { t->callback(); }));
        }
      }));
    }

    holder->release_ref();
  }

private:
  u32 num_threads = 0;
  std::vector<std::jthread> workers = {};
  std::deque<Arc<Job>> jobs = {};
  std::shared_mutex mutex = {};
  std::condition_variable_any condition_var = {};
  std::atomic<u64> job_count = {};
  bool running = true;
};

} // namespace ox
