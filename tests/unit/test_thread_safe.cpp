/**
 * @file tests/unit/test_thread_safe.cpp
 * @brief Test absolute-deadline queue waits.
 */
#include "../tests_common.h"

#include <chrono>

#include <src/thread_safe.h>

using namespace std::chrono_literals;

TEST(ThreadSafeQueueTest, PopUntilReturnsQueuedValue) {
  safe::queue_t<int> queue;
  queue.raise(42);

  auto value = queue.pop_until(std::chrono::steady_clock::now());

  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 42);
}

TEST(ThreadSafeQueueTest, PopUntilExpiresWithoutValue) {
  safe::queue_t<int> queue;

  auto value = queue.pop_until(std::chrono::steady_clock::now());

  EXPECT_FALSE(value);
}
