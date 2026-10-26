// Test runner entry point. Each tests/*_test.cpp adds its own TEST_CASEs.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

TEST_CASE("test harness runs") {
  CHECK(1 + 1 == 2);
}
