#pragma once
#include <mutex>

using Lock = std::lock_guard<std::mutex>;
