/**
 * @file src/platform/windows/vulkan_hdr_state.h
 * @brief Publish Sunshine HDR stream lifecycle to the Vulkan HDR layer.
 */
#pragma once

#include <cstdint>

namespace stream {
  struct session_t;
}

namespace platf::vulkan_hdr {
  void initialize();
  void set_pending(std::uint32_t launch_session_id, bool hdr_enabled);
  void clear_pending(std::uint32_t launch_session_id);
  void session_started(
    std::uint32_t launch_session_id,
    const stream::session_t *session,
    bool hdr_enabled
  );
  void session_stopped(const stream::session_t *session);
  void clear();
}  // namespace platf::vulkan_hdr
