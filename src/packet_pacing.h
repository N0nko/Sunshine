/**
 * @file src/packet_pacing.h
 * @brief Bounded, bitrate-aware packet scheduling without cross-frame pacing debt.
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace packet_pacing {
  /** @brief Pure schedule for one frame; the first batch is sent immediately. */
  struct schedule_t {
    std::uint64_t bytes_per_second;  ///< Wire-rate allowance, including FEC and framing headroom.
    std::uint64_t max_spread_us;  ///< Maximum requested delay, excluding OS scheduling overshoot.
    std::size_t batch_packets;  ///< Bounded syscall batching granularity.

    /**
     * @brief Compute an absolute offset, never a chained relative sleep.
     * @param sent_bytes Wire bytes already submitted for this frame.
     * @return Microseconds after this frame's send start.
     */
    std::uint64_t due_us(std::uint64_t sent_bytes) const {
      const auto capped_bytes = std::min(sent_bytes, bytes_per_second);
      return std::min(max_spread_us, capped_bytes * 1'000'000 / bytes_per_second);
    }
  };

  /**
   * @brief Make a low-latency burst schedule, not a bandwidth limiter.
   * @param bitrate_kbps Last acknowledged encoder bitrate (not an unconfirmed request).
   * @param fec_percent Configured parity overhead.
   * @param packet_bytes Complete estimated wire size of a packet.
   * @param frame_interval_us Negotiated frame period in microseconds.
   * @return Schedule with at most 2 ms or one quarter frame of intentional delay.
   */
  inline schedule_t make(int bitrate_kbps, int fec_percent, std::size_t packet_bytes, std::uint64_t frame_interval_us) {
    // Three times average wire bitrate leaves capacity to complete each frame early.
    // Framing/audio headroom is conservative; this does not redefine the bitrate slider.
    const auto kbps = static_cast<std::uint64_t>(std::clamp(bitrate_kbps, 500, 1'000'000));
    const auto overhead = static_cast<std::uint64_t>(110 + std::clamp(fec_percent, 0, 255));
    const auto rate = std::clamp<std::uint64_t>(kbps * 125 * 3 * overhead / 100, 1'000'000, 100'000'000);
    const auto bytes = std::clamp<std::size_t>(packet_bytes, 256, 65'536);
    const auto batch_limit = std::max<std::size_t>(1, std::min<std::size_t>(64, 65'535 / bytes));
    const auto batch = std::clamp<std::size_t>(rate / 2000 / bytes, 1, batch_limit);
    return {rate, std::min<std::uint64_t>(2000, frame_interval_us / 4), batch};
  }
}  // namespace packet_pacing
