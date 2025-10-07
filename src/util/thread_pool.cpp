#include "util/thread_pool.hpp"

ThreadPool::ThreadPool(size_t numThreads) {
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this]() {
      while (true) {
        std::function<void()> job;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [this]() { return stop || !jobs.empty(); });
          if (stop && jobs.empty())
            return;
          job = std::move(jobs.front());
          jobs.pop();
        }
        job();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (auto &worker : workers)
    worker.join();
}

void ThreadPool::enqueue(std::function<void()> job) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    jobs.push(std::move(job));
  }
  condition.notify_one();
}
