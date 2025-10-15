#pragma once

#include <mutex>
#include <shared_mutex>

using Lock = std::lock_guard<std::mutex>;
using SharedMutex = std::shared_mutex;
using ReadLock = std::shared_lock<SharedMutex>;
using WriteLock = std::unique_lock<SharedMutex>;
