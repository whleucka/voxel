#pragma once

#include <cstddef>
#include <sys/resource.h>


inline size_t getMemoryUsage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss / 1024.0f; // MB
}
