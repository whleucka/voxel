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

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> jobs;

  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop = false;
};
