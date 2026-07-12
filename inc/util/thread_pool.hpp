#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
  ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
  ~ThreadPool();

  template <class F> void enqueue(F &&f) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      jobs.push(std::function<void()>(std::forward<F>(f)));
    }
    condition.notify_one();
  }

  // Stops the pool: lets in-flight jobs finish, drops queued ones, joins all
  // workers. Idempotent. Call before destroying anything the jobs touch.
  void shutdown();

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> jobs;

  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop = false;
};
