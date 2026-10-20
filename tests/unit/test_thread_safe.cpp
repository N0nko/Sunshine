/**
 * @file tests/unit/test_thread_safe.cpp
 * @brief Tests for thread-safe utility types.
 */

// standard includes
#include <cstdint>
#include <future>
#include <type_traits>

// lib includes
#include <gtest/gtest.h>

// local includes
#include "src/stream_ping.h"
#include "src/thread_safe.h"

TEST(ThreadSafeQueue, SessionStopWakesBothInitialPingWaits) {
  using namespace std::chrono_literals;
  stream::startup_ping_queues_t<int> pings;
  std::promise<void> video_waiting;
  std::promise<void> audio_waiting;
  auto video = std::async(std::launch::async, [&]() {
    video_waiting.set_value();
    return pings.video->pop(25s);
  });
  auto audio = std::async(std::launch::async, [&]() {
    audio_waiting.set_value();
    return pings.audio->pop(25s);
  });
  video_waiting.get_future().wait();
  audio_waiting.get_future().wait();
  pings.stop();
  EXPECT_EQ(video.wait_for(2s), std::future_status::ready);
  EXPECT_EQ(audio.wait_for(2s), std::future_status::ready);
  EXPECT_FALSE(video.get());
  EXPECT_FALSE(audio.get());
  EXPECT_FALSE(pings.video->raise(1));
  EXPECT_FALSE(pings.audio->raise(1));
}

TEST(ThreadSafeQueue, SessionCancelledBeforeWorkersStartDoesNotWaitForPing) {
  using namespace std::chrono_literals;
  stream::startup_ping_queues_t<int> pings;
  pings.stop();
  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(pings.video->pop(25s));
  EXPECT_FALSE(pings.audio->pop(25s));
  EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
}

static_assert(!std::is_convertible_v<std::uint32_t, safe::queue_t<int>>);

TEST(ThreadSafeQueue, RejectsNewestItemAtCapacity) {
  safe::queue_t<int> queue {1, safe::queue_t<int>::overflow_policy_e::reject};

  ASSERT_TRUE(queue.raise(1));
  EXPECT_FALSE(queue.raise(2));

  const auto value = queue.pop();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 1);
}

TEST(ThreadSafeQueue, DropsQueuedItemsByDefaultAtCapacity) {
  safe::queue_t<int> queue {2};

  ASSERT_TRUE(queue.raise(1));
  ASSERT_TRUE(queue.raise(2));
  ASSERT_TRUE(queue.raise(3));

  const auto value = queue.pop();
  ASSERT_TRUE(value);
  EXPECT_EQ(*value, 3);
}

TEST(ThreadSafeQueue, RejectsItemsAfterStop) {
  safe::queue_t<int> queue;

  queue.stop();

  EXPECT_FALSE(queue.raise(1));
}

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
