/** @file tests/unit/test_packet_pacing.cpp
 * @brief Deterministic packet pacing bounds and malformed-input tests.
 */
#include "src/packet_pacing.h"

#include <gtest/gtest.h>
#include <limits>

TEST(PacketPacing, FirstBatchIsImmediateAndDelayIsBounded) {
  const auto p = packet_pacing::make(40'000, 20, 1464, 11111);
  EXPECT_EQ(p.due_us(0), 0u);
  EXPECT_EQ(p.max_spread_us, 2000u);
  EXPECT_LT(p.batch_packets, 64u * 1024 / 1464);
  EXPECT_LE(p.due_us(1'000'000), 2000u);
  EXPECT_EQ(p.due_us(std::numeric_limits<std::uint64_t>::max()), 2000u);
}

TEST(PacketPacing, MonotonicAcrossRatesAndIndependentFrames) {
  for (int rate : {500, 2000, 20000, 40000, 80000, 200000, 300000, 1000000}) {
    for (int fec : {0, 20, 100, 255}) {
      for (std::size_t size : {256, 1200, 1464, 65536}) {
        auto p = packet_pacing::make(rate, fec, size, 11111);
        std::uint64_t previous = 0;
        for (std::uint64_t sent = 0; sent < 2'000'000; sent += size) {
          const auto now = p.due_us(sent);
          EXPECT_GE(now, previous);
          EXPECT_LE(now, 2000u);
          previous = now;
        }
        EXPECT_GE(p.batch_packets, 1u);
        EXPECT_LE(p.batch_packets, 64u);
        EXPECT_EQ(packet_pacing::make(rate, fec, size, 11111).due_us(0), 0u);
      }
    }
  }
}

TEST(PacketPacing, HighRefreshAndInvalidInputsStayBounded) {
  EXPECT_EQ(packet_pacing::make(40000, 20, 1400, 4000).max_spread_us, 1000u);
  EXPECT_EQ(packet_pacing::make(0, -1, 0, 0).due_us(10000), 0u);
  const auto extreme = packet_pacing::make(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<std::size_t>::max(), 11111);
  EXPECT_GT(extreme.bytes_per_second, 0u);
  EXPECT_EQ(extreme.batch_packets, 1u);
  EXPECT_LE(extreme.due_us(100000), 2000u);
}

TEST(PacketPacing, AppliedBitrateChangesTheSchedule) {
  auto slow = packet_pacing::make(20000, 20, 1464, 11111);
  auto fast = packet_pacing::make(200000, 20, 1464, 11111);
  EXPECT_LT(slow.bytes_per_second, fast.bytes_per_second);
  EXPECT_GT(slow.due_us(10000), fast.due_us(10000));
  EXPECT_LT(slow.batch_packets, fast.batch_packets);
}
