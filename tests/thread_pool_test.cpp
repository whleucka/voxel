#include "doctest/doctest.h"
#include "util/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

TEST_CASE("every queued job runs") {
  std::atomic<int> ran{0};
  {
    ThreadPool pool(3);
    for (int i = 0; i < 50; ++i) pool.enqueue([&] { ++ran; });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (ran < 50 && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(ran == 50);
}

TEST_CASE("a job that throws doesn't take the process or the pool down") {
  std::atomic<int> ran{0};
  ThreadPool pool(1); // one worker: the job after the throw needs it alive
  pool.enqueue([] { throw std::runtime_error("a bad chunk"); });
  pool.enqueue([&] { ++ran; });
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (ran == 0 && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(ran == 1);
}

TEST_CASE("shutdown is idempotent and drops what's still queued") {
  ThreadPool pool(1);
  pool.shutdown();
  pool.shutdown();
  pool.enqueue([] { FAIL("ran after shutdown"); });
}
