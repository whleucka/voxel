#include "util/thread_pool.hpp"

ThreadPool::ThreadPool(size_t numThreads) {
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this]() {
      while (true) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [this]() { return stop || !jobs.empty(); });
          // On shutdown, finish the current job (already popped) but drop any
          // still queued — they touch world state that is about to be freed.
          if (stop)
            return;
          job = std::move(jobs.front());
          jobs.pop();
        }
        job();
      }
    });
  }
}

void ThreadPool::shutdown() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (stop)
      return; // already shut down
    stop = true;
  }
  condition.notify_all();
  for (auto &worker : workers)
    if (worker.joinable())
      worker.join();
}

ThreadPool::~ThreadPool() { shutdown(); }
