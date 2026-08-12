/**
 * @file src/remote_display.h
 * @brief Allowlisted bridge to the local RemoteDisplayControl service.
 */
#pragma once

#include <cstdint>
#include <string>

namespace remote_display {
  /**
   * @brief Allowlisted display profiles exposed to the remote client.
   */
  enum class profile_e : std::uint8_t {
    desk = 1,  ///< Local desk display profile.
    remote = 2,  ///< Steam Deck remote-play display profile.
    tv = 3,  ///< Television display profile.
  };

  /**
   * @brief Result returned by the local display controller.
   */
  enum class status_e : std::uint8_t {
    applied = 2,  ///< The requested profile was applied.
    unavailable = 3,  ///< The localhost controller could not be reached.
    failed = 4,  ///< The controller rejected or failed the request.
  };

  /**
   * @brief Correlated result sent back over the encrypted control stream.
   */
  struct result_t {
    std::uint32_t request_id;  ///< Client-generated request identifier.
    profile_e profile;  ///< Profile associated with this result.
    status_e status;  ///< Application result.
  };

  /**
   * @brief Start a generation and invalidate delayed work from older sessions.
   * @param client_id Paired-client certificate identity.
   * @param generation RTSP launch generation for this stream.
   */
  void session_connected(
    const std::string &client_id,
    std::uint64_t generation
  );

  /**
   * @brief Set startup and disconnect behavior for the current session.
   * @param client_id Paired-client certificate identity.
   * @param generation Current stream lifecycle generation.
   * @param apply_remote_on_connect Apply the Remote profile on cold start.
   * @param restore_desk_on_disconnect Restore Desk after disconnect.
   */
  void set_policy(
    const std::string &client_id,
    std::uint64_t generation,
    bool apply_remote_on_connect,
    bool restore_desk_on_disconnect
  );

  /**
   * @brief Apply an allowlisted profile while the originating session is current.
   * @param client_id Paired-client certificate identity.
   * @param generation Current stream lifecycle generation.
   * @param profile Allowlisted profile.
   * @return Local controller result.
   */
  status_e apply_for_session(
    const std::string &client_id,
    std::uint64_t generation,
    profile_e profile
  );

  /**
   * @brief Handle display restoration after a session disconnect.
   * @param client_id Paired-client certificate identity.
   * @param generation Disconnecting stream lifecycle generation.
   * @param intentional Whether the client marked a clean shutdown.
   */
  void session_disconnected(
    const std::string &client_id,
    std::uint64_t generation,
    bool intentional
  );
}  // namespace remote_display
