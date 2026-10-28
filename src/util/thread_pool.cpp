#include "util/thread_pool.hpp"
#include <exception>
#include <iostream>

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
        // A job that throws loses its own work, not the process: an
        // exception escaping a worker thread would end the game unsaved.
        try {
          job();
        } catch (const std::exception &e) {
          std::cerr << "[pool] a background job failed: " << e.what() << "\n";
        } catch (...) {
          std::cerr << "[pool] a background job failed\n";
        }
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
