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

TEST(DeckProtocol, ParsesAllowlistedDisplayRequest) {
  std::array<std::uint8_t, deck_protocol::header_size + 4> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::remote_display,
    deck_protocol::display::apply,
    9,
    4
  ));
  wire[deck_protocol::header_size] =
    static_cast<std::uint8_t>(deck_protocol::display::profile_e::remote);

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  const auto profile = deck_protocol::parse_display_apply(*message);
  ASSERT_TRUE(profile);
  EXPECT_EQ(*profile, deck_protocol::display::profile_e::remote);

  wire[deck_protocol::header_size] = 4;
  const auto rejected = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(rejected);
  EXPECT_FALSE(deck_protocol::parse_display_apply(*rejected));
}

TEST(DeckProtocol, ParsesDisplayLifecyclePolicy) {
  std::array<std::uint8_t, deck_protocol::header_size + 4> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::remote_display,
    deck_protocol::display::policy,
    0,
    4
  ));
  wire[deck_protocol::header_size] = 0x03;

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  const auto policy = deck_protocol::parse_display_policy(*message);
  ASSERT_TRUE(policy);
  EXPECT_TRUE(policy->apply_remote_on_connect);
  EXPECT_TRUE(policy->restore_desk_on_disconnect);
}

TEST(DeckProtocol, EncodesDisplayResult) {
  const auto payload = deck_protocol::make_display_result(
    deck_protocol::display::status_e::applied,
    deck_protocol::display::profile_e::tv
  );
  EXPECT_EQ(payload[0], static_cast<std::uint8_t>(deck_protocol::display::status_e::applied));
  EXPECT_EQ(payload[1], static_cast<std::uint8_t>(deck_protocol::display::profile_e::tv));
  EXPECT_EQ(payload[2], 0);
  EXPECT_EQ(payload[3], 0);
}

TEST(DeckProtocol, ParsesMicrophoneConfiguration) {
  std::array<std::uint8_t, deck_protocol::header_size + 8> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::deck_microphone,
    deck_protocol::microphone::configure,
    7,
    8
  ));
  auto *payload = wire.data() + deck_protocol::header_size;
  payload[0] = 1;
  payload[1] = deck_protocol::microphone::opus_codec;
  payload[2] = deck_protocol::microphone::channel_count;
  deck_protocol::write_le32(payload + 4, deck_protocol::microphone::sample_rate);

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  const auto config = deck_protocol::parse_microphone_config(*message);
  ASSERT_TRUE(config);
  EXPECT_TRUE(config->enabled);
  EXPECT_EQ(config->codec, deck_protocol::microphone::opus_codec);
  EXPECT_EQ(config->channels, deck_protocol::microphone::channel_count);
  EXPECT_EQ(config->sample_rate, deck_protocol::microphone::sample_rate);
}

TEST(DeckProtocol, ParsesMicrophoneOpusFrame) {
  std::array<std::uint8_t, deck_protocol::header_size + 6> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::deck_microphone,
    deck_protocol::microphone::opus,
    19,
    6
  ));
  auto *payload = wire.data() + deck_protocol::header_size;
  deck_protocol::write_le16(payload, deck_protocol::microphone::frame_samples);
  payload[2] = 0x11;
  payload[3] = 0x22;
  payload[4] = 0x33;
  payload[5] = 0x44;

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  const auto opus = deck_protocol::parse_microphone_opus(*message);
  ASSERT_TRUE(opus);
  EXPECT_EQ(opus->size(), 4U);
  EXPECT_EQ(static_cast<std::uint8_t>((*opus)[0]), 0x11U);
}

TEST(DeckProtocol, RejectsMalformedMicrophoneMessages) {
  std::array<std::uint8_t, deck_protocol::header_size + 8> wire {};
  ASSERT_TRUE(deck_protocol::write_header(
    wire.data(),
    wire.size(),
    deck_protocol::feature_e::deck_microphone,
    deck_protocol::microphone::configure,
    1,
    8
  ));
  auto *payload = wire.data() + deck_protocol::header_size;
  payload[0] = 2;

  const auto message = deck_protocol::parse(std::string_view {
    reinterpret_cast<const char *>(wire.data()), wire.size()
  });
  ASSERT_TRUE(message);
  EXPECT_FALSE(deck_protocol::parse_microphone_config(*message));
}
