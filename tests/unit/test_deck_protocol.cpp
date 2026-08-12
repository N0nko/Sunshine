/**
 * @file tests/unit/test_deck_protocol.cpp
 * @brief Tests for the Deck encrypted extension wire schema.
 */

#include <gtest/gtest.h>

#include "src/deck_protocol.h"

TEST(DeckProtocol, ParsesLiveBitrateRequest) {
  std::array<std::uint8_t, deck_protocol::header_size + sizeof(std::uint32_t)> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::live_bitrate,
    deck_protocol::bitrate::set,
    42,
    sizeof(std::uint32_t)
  ));
  deck_protocol::write_le32(wire.data() + deck_protocol::header_size, 300000);

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  EXPECT_EQ(message->request_id, 42U);
  EXPECT_EQ(deck_protocol::parse_bitrate_request(*message), 300000U);
}

TEST(DeckProtocol, RejectsMalformedEnvelope) {
  std::array<std::uint8_t, deck_protocol::header_size> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::live_bitrate,
    deck_protocol::bitrate::set,
    1,
    0
  ));

  wire[3] = 1;
  EXPECT_FALSE(deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  }));
  wire[3] = 0;
  deck_protocol::write_le16(wire.data() + 8, 1);
  EXPECT_FALSE(deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  }));
}

TEST(DeckProtocol, RejectsUndersizedDestination) {
  std::array<std::uint8_t, deck_protocol::header_size> wire {};
  EXPECT_FALSE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::live_bitrate,
    deck_protocol::bitrate::set,
    1,
    sizeof(std::uint32_t)
  ));
}

TEST(DeckProtocol, EncodesBitrateResult) {
  const auto payload = deck_protocol::make_bitrate_result(
    deck_protocol::bitrate::status_e::clamped,
    40000
  );

  EXPECT_EQ(payload[0], static_cast<std::uint8_t>(deck_protocol::bitrate::status_e::clamped));
  EXPECT_EQ(deck_protocol::read_le32(payload.data() + 4), 40000U);
}
