/**
 * @file tests/unit/test_thread_safe.cpp
 * @brief Test absolute-deadline queue waits.
 */
#include "../tests_common.h"

#include <chrono>

#include <src/thread_safe.h>

using namespace std::chrono_literals;

TEST(ThreadSafeEventTest, PopUntilReturnsRaisedValue) {
  safe::event_t<int> event;
  event.raise(42);

  auto value = event.pop_until(std::chrono::steady_clock::now());

  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 42);
}

TEST(ThreadSafeEventTest, PopUntilExpiresWithoutValue) {
  safe::event_t<int> event;

  auto value = event.pop_until(std::chrono::steady_clock::now());

  EXPECT_FALSE(value);
}
