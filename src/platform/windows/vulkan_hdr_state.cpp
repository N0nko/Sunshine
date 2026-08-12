/**
 * @file src/platform/windows/vulkan_hdr_state.cpp
 * @brief Publish Sunshine HDR stream lifecycle to the Vulkan HDR layer.
 */

#include "vulkan_hdr_state.h"

#include <array>
#include <mutex>
#include <optional>
#include <unordered_set>

#include <windows.h>
#include <sddl.h>

#include "src/logging.h"

namespace platf::vulkan_hdr {
  namespace {
    constexpr wchar_t global_event_name[] = L"Global\\SunshineVirtualHdrActive";
    constexpr wchar_t local_event_name[] = L"Local\\SunshineVirtualHdrActive";

    struct publisher_t {
      std::mutex mutex;
      std::unordered_set<std::uint32_t> pending_sessions;
      std::unordered_set<const stream::session_t *> active_sessions;
      HANDLE global_event = nullptr;
      HANDLE local_event = nullptr;
      bool events_initialized = false;
      std::optional<bool> published_state;

      ~publisher_t() {
        if (global_event != nullptr) {
          CloseHandle(global_event);
        }
        if (local_event != nullptr) {
          CloseHandle(local_event);
        }
      }
    };

    publisher_t &publisher() {
      static publisher_t instance;
      return instance;
    }

    HANDLE create_event(const wchar_t *name) {
      PSECURITY_DESCRIPTOR descriptor = nullptr;
      SECURITY_ATTRIBUTES attributes {};
      attributes.nLength = sizeof(attributes);

      SECURITY_ATTRIBUTES *attributes_ptr = nullptr;
      if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;WD)",
            SDDL_REVISION_1,
            &descriptor,
            nullptr
          )) {
        attributes.lpSecurityDescriptor = descriptor;
        attributes_ptr = &attributes;
      }

      HANDLE event = CreateEventW(attributes_ptr, TRUE, FALSE, name);
      if (descriptor != nullptr) {
        LocalFree(descriptor);
      }
      return event;
    }

    void ensure_events(publisher_t &state) {
      if (state.events_initialized) {
        return;
      }
      state.events_initialized = true;
      state.global_event = create_event(global_event_name);
      state.local_event = create_event(local_event_name);
      if (state.global_event == nullptr && state.local_event == nullptr) {
        BOOST_LOG(warning) << "Unable to create Vulkan HDR stream events";
      }
    }

    void publish_locked(publisher_t &state) {
      ensure_events(state);
      const bool active = !state.pending_sessions.empty() ||
                          !state.active_sessions.empty();
      if (state.published_state && *state.published_state == active) {
        return;
      }

      bool updated = false;
      bool failed = false;
      for (HANDLE event : std::array {state.global_event, state.local_event}) {
        if (event == nullptr) {
          continue;
        }
        const BOOL result = active ? SetEvent(event) : ResetEvent(event);
        updated = result || updated;
        failed = !result || failed;
      }

      if (!updated || failed) {
        BOOST_LOG(warning) << "Unable to publish Vulkan HDR stream state";
        return;
      }

      state.published_state = active;
      BOOST_LOG(info) << "Vulkan HDR stream signal " << (active ? "active" : "inactive");
    }
  }  // namespace

  void initialize() {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    publish_locked(state);
  }

  void set_pending(std::uint32_t launch_session_id, bool hdr_enabled) {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    if (hdr_enabled) {
      state.pending_sessions.insert(launch_session_id);
    } else {
      state.pending_sessions.erase(launch_session_id);
    }
    publish_locked(state);
  }

  void clear_pending(std::uint32_t launch_session_id) {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    state.pending_sessions.erase(launch_session_id);
    publish_locked(state);
  }

  void session_started(
    std::uint32_t launch_session_id,
    const stream::session_t *session,
    bool hdr_enabled
  ) {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    state.pending_sessions.erase(launch_session_id);
    if (hdr_enabled) {
      state.active_sessions.insert(session);
    } else {
      state.active_sessions.erase(session);
    }
    publish_locked(state);
  }

  void session_stopped(const stream::session_t *session) {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    state.active_sessions.erase(session);
    publish_locked(state);
  }

  void clear() {
    auto &state = publisher();
    std::lock_guard lock(state.mutex);
    state.pending_sessions.clear();
    state.active_sessions.clear();
    publish_locked(state);
  }
}  // namespace platf::vulkan_hdr
