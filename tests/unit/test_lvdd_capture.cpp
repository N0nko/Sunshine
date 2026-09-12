/** @file tests/unit/test_lvdd_capture.cpp
 * @brief Wire-contract and newest-frame policy tests, independent of driver availability.
 */
#include "src/platform/windows/lvdd_capture_protocol.h"

#include <gtest/gtest.h>
#include <limits>

TEST(LvddCapture, RejectsIncompatibleRequests) {
  lvdd_capture::request r;
  EXPECT_TRUE(lvdd_capture::valid(r));
  ++r.protocol;
  EXPECT_FALSE(lvdd_capture::valid(r));
  r = {};
  --r.size;
  EXPECT_FALSE(lvdd_capture::valid(r));
  r = {};
  r.reserved = 1;
  EXPECT_FALSE(lvdd_capture::valid(r));
}

TEST(LvddCapture, PicksNewestNotNextFifoSlot) {
  lvdd_capture::frame entries[lvdd_capture::slots] {};
  EXPECT_EQ(lvdd_capture::latest(entries), 0u);
  entries[2].sequence = 10;
  EXPECT_EQ(lvdd_capture::latest(entries), 2u);
  entries[0].sequence = 12;
  EXPECT_EQ(lvdd_capture::latest(entries), 0u);
  entries[1].sequence = 11;
  EXPECT_EQ(lvdd_capture::latest(entries), 0u);
  entries[2].sequence = std::numeric_limits<std::uint64_t>::max();
  EXPECT_EQ(lvdd_capture::latest(entries), 2u);
}

TEST(LvddCapture, MetadataContainsNoPixelBufferOrProcessPointers) {
  EXPECT_EQ(sizeof(lvdd_capture::shared), 128u);
  EXPECT_EQ(sizeof(lvdd_capture::reply), 64u);
  EXPECT_EQ(lvdd_capture::attach_ioctl & 3, 0u);
  EXPECT_EQ((lvdd_capture::attach_ioctl >> 14) & 3, 3u);
}
