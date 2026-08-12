/**
 * @file src/platform/windows/deck_microphone.h
 * @brief Native Steam Deck microphone sink for Windows.
 */
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace platf::deck_microphone {
  /**
   * @brief Wire codec selected by the Moonlight client.
   */
  enum class codec_e : std::uint8_t {
    pcm_s16 = 0,  ///< Legacy 16 kHz signed 16-bit PCM.
    opus = 1,  ///< 48 kHz mono Opus.
  };

  /**
   * @brief Result of opening or controlling the host microphone sink.
   */
  enum class status_e : std::uint8_t {
    active = 2,  ///< The WASAPI sink is accepting Deck microphone audio.
    unsupported = 3,  ///< The requested render endpoint is unavailable.
    failed = 4,  ///< WASAPI initialization failed.
    disabled = 5,  ///< The client explicitly disabled microphone forwarding.
  };

  /**
   * @brief Start the sink for the current paired client generation.
   * @param owner Paired-client certificate identity.
   * @param generation Current stream lifecycle generation.
   * @param codec Negotiated wire codec.
   * @param sample_rate Negotiated output sample rate.
   * @return Native sink startup status.
   */
  status_e start(
    const std::string &owner,
    std::uint64_t generation,
    codec_e codec,
    std::uint32_t sample_rate
  );

  /**
   * @brief Queue one ordered block of legacy signed 16-bit mono PCM.
   * @param owner Paired-client certificate identity.
   * @param generation Current stream lifecycle generation.
   * @param sequence Client packet sequence.
   * @param samples 16 kHz microphone samples.
   */
  void submit_pcm(
    const std::string &owner,
    std::uint64_t generation,
    std::uint32_t sequence,
    std::span<const std::int16_t> samples
  );

  /**
   * @brief Decode and queue one ordered 20 ms mono Opus packet.
   * @param owner Paired-client certificate identity.
   * @param generation Current stream lifecycle generation.
   * @param sequence Client packet sequence.
   * @param packet Opus payload containing 960 samples at 48 kHz.
   */
  void submit_opus(
    const std::string &owner,
    std::uint64_t generation,
    std::uint32_t sequence,
    std::span<const std::uint8_t> packet
  );

  /**
   * @brief Stop the sink only if the owner and generation are still current.
   * @param owner Paired-client certificate identity.
   * @param generation Stream lifecycle generation to stop.
   */
  void stop(const std::string &owner, std::uint64_t generation);
}  // namespace platf::deck_microphone
