/**
 * @file src/deck_protocol.h
 * @brief Wire schema for Deck-specific encrypted control extensions.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace deck_protocol {
  constexpr std::uint16_t control_packet_type = 0x55A0;
  constexpr std::uint8_t version = 1;
  constexpr std::size_t header_size = 12;
  constexpr std::size_t max_payload_size = 224;

  enum class feature_e : std::uint8_t {
    live_bitrate = 1,
    deck_microphone = 2,
    remote_display = 3,
  };

  struct message_view_t {
    feature_e feature;
    std::uint8_t opcode;
    std::uint32_t request_id;
    std::string_view payload;
  };

  namespace bitrate {
    constexpr std::uint8_t set = 1;
    constexpr std::uint8_t result = 2;
    constexpr std::uint32_t minimum_kbps = 500;
    constexpr std::uint32_t maximum_kbps = 500000;

    enum class status_e : std::uint8_t {
      applied = 2,
      clamped = 3,
      failed = 4,
      unsupported = 5,
    };
  }  // namespace bitrate

  namespace display {
    constexpr std::uint8_t apply = 1;
    constexpr std::uint8_t result = 2;
    constexpr std::uint8_t policy = 3;
    constexpr std::uint8_t intentional_disconnect = 4;

    enum class profile_e : std::uint8_t {
      desk = 1,
      remote = 2,
      tv = 3,
    };

    enum class status_e : std::uint8_t {
      applied = 2,
      unavailable = 3,
      failed = 4,
    };

    struct policy_t {
      bool apply_remote_on_connect;
      bool restore_desk_on_disconnect;
    };
  }  // namespace display

  namespace microphone {
    constexpr std::uint8_t configure = 1;
    constexpr std::uint8_t status = 2;
    constexpr std::uint8_t opus = 3;
    constexpr std::uint8_t opus_codec = 1;
    constexpr std::uint8_t channel_count = 1;
    constexpr std::uint32_t sample_rate = 48000;
    constexpr std::uint16_t frame_samples = 960;

    enum class status_e : std::uint8_t {
      active = 2,
      unsupported = 3,
      failed = 4,
      disabled = 5,
    };

    struct config_t {
      bool enabled;
      std::uint8_t codec;
      std::uint8_t channels;
      std::uint32_t sample_rate;
    };
  }  // namespace microphone

  constexpr std::uint16_t read_le16(const std::uint8_t *data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(data[1]) << 8;
  }

  constexpr std::uint32_t read_le32(const std::uint8_t *data) {
    return static_cast<std::uint32_t>(data[0]) |
           static_cast<std::uint32_t>(data[1]) << 8 |
           static_cast<std::uint32_t>(data[2]) << 16 |
           static_cast<std::uint32_t>(data[3]) << 24;
  }

  constexpr void write_le16(std::uint8_t *data, std::uint16_t value) {
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8);
  }

  constexpr void write_le32(std::uint8_t *data, std::uint32_t value) {
    data[0] = static_cast<std::uint8_t>(value);
    data[1] = static_cast<std::uint8_t>(value >> 8);
    data[2] = static_cast<std::uint8_t>(value >> 16);
    data[3] = static_cast<std::uint8_t>(value >> 24);
  }

  inline std::optional<message_view_t> parse(std::string_view data) {
    if (data.size() < header_size) {
      return std::nullopt;
    }

    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data.data());
    const std::uint16_t payload_size = read_le16(bytes + 8);
    if (bytes[0] != version || bytes[3] != 0 || read_le16(bytes + 10) != 0 ||
        payload_size > max_payload_size || data.size() != header_size + payload_size) {
      return std::nullopt;
    }

    return message_view_t {
      static_cast<feature_e>(bytes[1]),
      bytes[2],
      read_le32(bytes + 4),
      data.substr(header_size, payload_size),
    };
  }

  inline bool write_header(
    std::uint8_t *destination,
    std::size_t destination_size,
    feature_e feature,
    std::uint8_t opcode,
    std::uint32_t request_id,
    std::size_t payload_size
  ) {
    if (payload_size > max_payload_size ||
        destination_size < header_size + payload_size) {
      return false;
    }

    destination[0] = version;
    destination[1] = static_cast<std::uint8_t>(feature);
    destination[2] = opcode;
    destination[3] = 0;
    write_le32(destination + 4, request_id);
    write_le16(destination + 8, static_cast<std::uint16_t>(payload_size));
    write_le16(destination + 10, 0);
    return true;
  }

  inline std::optional<std::uint32_t> parse_bitrate_request(const message_view_t &message) {
    if (message.feature != feature_e::live_bitrate ||
        message.opcode != bitrate::set ||
        message.request_id == 0 ||
        message.payload.size() != sizeof(std::uint32_t)) {
      return std::nullopt;
    }

    return read_le32(reinterpret_cast<const std::uint8_t *>(message.payload.data()));
  }

  inline std::array<std::uint8_t, 8> make_bitrate_result(
    bitrate::status_e status,
    std::uint32_t applied_kbps
  ) {
    std::array<std::uint8_t, 8> payload {};
    payload[0] = static_cast<std::uint8_t>(status);
    write_le32(payload.data() + 4, applied_kbps);
    return payload;
  }

  inline std::optional<display::profile_e> parse_display_apply(
    const message_view_t &message
  ) {
    if (message.feature != feature_e::remote_display ||
        message.opcode != display::apply ||
        message.request_id == 0 ||
        message.payload.size() != 4) {
      return std::nullopt;
    }

    const auto *payload = reinterpret_cast<const std::uint8_t *>(message.payload.data());
    if (payload[1] != 0 || payload[2] != 0 || payload[3] != 0 ||
        payload[0] < static_cast<std::uint8_t>(display::profile_e::desk) ||
        payload[0] > static_cast<std::uint8_t>(display::profile_e::tv)) {
      return std::nullopt;
    }
    return static_cast<display::profile_e>(payload[0]);
  }

  inline std::optional<display::policy_t> parse_display_policy(
    const message_view_t &message
  ) {
    if (message.feature != feature_e::remote_display ||
        message.opcode != display::policy ||
        message.request_id != 0 ||
        message.payload.size() != 4) {
      return std::nullopt;
    }

    const auto *payload = reinterpret_cast<const std::uint8_t *>(message.payload.data());
    if ((payload[0] & ~0x03U) != 0 ||
        payload[1] != 0 || payload[2] != 0 || payload[3] != 0) {
      return std::nullopt;
    }
    return display::policy_t {
      (payload[0] & 0x01U) != 0,
      (payload[0] & 0x02U) != 0,
    };
  }

  inline bool is_intentional_display_disconnect(const message_view_t &message) {
    return message.feature == feature_e::remote_display &&
           message.opcode == display::intentional_disconnect &&
           message.request_id == 0 &&
           message.payload.empty();
  }

  inline std::array<std::uint8_t, 4> make_display_result(
    display::status_e status,
    display::profile_e profile
  ) {
    std::array<std::uint8_t, 4> payload {};
    payload[0] = static_cast<std::uint8_t>(status);
    payload[1] = static_cast<std::uint8_t>(profile);
    return payload;
  }

  inline std::optional<microphone::config_t> parse_microphone_config(
    const message_view_t &message
  ) {
    if (message.feature != feature_e::deck_microphone ||
        message.opcode != microphone::configure ||
        message.request_id == 0 ||
        message.payload.size() != 8) {
      return std::nullopt;
    }

    const auto *payload = reinterpret_cast<const std::uint8_t *>(message.payload.data());
    if (payload[0] > 1 || payload[3] != 0) {
      return std::nullopt;
    }

    return microphone::config_t {
      payload[0] != 0,
      payload[1],
      payload[2],
      read_le32(payload + 4),
    };
  }

  inline std::optional<std::string_view> parse_microphone_opus(
    const message_view_t &message
  ) {
    if (message.feature != feature_e::deck_microphone ||
        message.opcode != microphone::opus ||
        message.request_id == 0 ||
        message.payload.size() <= sizeof(std::uint16_t) ||
        read_le16(reinterpret_cast<const std::uint8_t *>(message.payload.data())) !=
          microphone::frame_samples) {
      return std::nullopt;
    }

    return message.payload.substr(sizeof(std::uint16_t));
  }

  inline std::array<std::uint8_t, 4> make_microphone_status(
    microphone::status_e status
  ) {
    std::array<std::uint8_t, 4> payload {};
    payload[0] = static_cast<std::uint8_t>(status);
    return payload;
  }
}  // namespace deck_protocol
