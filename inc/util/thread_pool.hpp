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

  void enqueue(std::function<void()> job);

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> jobs;

  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop = false;
};