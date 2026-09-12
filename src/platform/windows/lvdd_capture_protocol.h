/**
 * @file capture_protocol.h
 * @brief Private, versioned LVDD GPU capture contract; mirrored in Sunshine.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace lvdd_capture {
  inline constexpr std::uint32_t version = 1; ///< Incompatible wire changes increment this value.
  inline constexpr std::uint32_t attach_ioctl = (0x22u << 16) | (3u << 14) | (0x940u << 2); ///< Restricted METHOD_BUFFERED IOCTL.
  inline constexpr std::uint32_t slots = 3; ///< Overwriteable mailbox, not a presentation queue.

  /** @brief Request for one OS target; handles remain pinned until its device file closes. */
  struct request {
    std::uint32_t size {sizeof(request)};
    std::uint32_t protocol {version};
    std::uint32_t target_id {};
    std::uint32_t reserved {};
    std::uint32_t adapter_low {};
    std::int32_t adapter_high {};
  };
  /** @brief Unnamed NT handles in producer_pid, duplicated by the SYSTEM capture client. */
  struct reply {
    std::uint32_t size {sizeof(reply)};
    std::uint32_t protocol {version};
    std::uint32_t producer_pid {};
    std::uint32_t mapping_bytes {};
    std::uint64_t mapping {};
    std::uint64_t lock {};
    std::uint64_t event {};
    std::uint64_t textures[slots] {};
  };
  /** @brief Metadata protected by the shared mutex; pixels are never CPU mapped. */
  struct frame {
    std::uint64_t sequence {};
    std::uint64_t present_qpc {};
    std::uint64_t acquire_qpc {};
  };
  /** @brief Fixed-layout metadata; all fields accessed under lock except atomic state. */
  struct shared {
    std::uint32_t size {sizeof(shared)}, protocol {version};
    volatile std::int32_t state {1}; ///< Aligned Windows interprocess state, written with InterlockedExchange.
    std::uint32_t width {}, height {}, format {}, color_space {}, sdr_white_nits {};
    std::uint32_t adapter_low {};
    std::int32_t adapter_high {};
    std::uint64_t produced {}, busy_drops {};
    frame frames[slots] {};
  };
  /** @brief Validate the request before looking up monitor state. */
  constexpr bool valid(const request &r) {
    return r.size == sizeof(request) && r.protocol == version && !r.reserved;
  }
  /** @brief Select the latest mailbox entry without introducing FIFO buffering. */
  constexpr unsigned latest(const frame (&entries)[slots]) {
    unsigned result = 0;
    for (unsigned i = 1; i < slots; ++i)
      if (entries[i].sequence > entries[result].sequence) result = i;
    return result;
  }
  static_assert(sizeof(request) == 24 && sizeof(reply) == 64);
  static_assert(sizeof(frame) == 24 && offsetof(shared, frames) == 56);
  static_assert(std::is_standard_layout_v<shared> && std::is_trivially_copyable_v<shared>);
}
