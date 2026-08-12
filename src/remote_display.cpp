/**
 * @file src/remote_display.cpp
 * @brief Allowlisted bridge to the local RemoteDisplayControl service.
 */

#include "remote_display.h"

#include <chrono>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include <curl/curl.h>

#include "globals.h"
#include "logging.h"

using namespace std::chrono_literals;

namespace remote_display {
  namespace {
    constexpr auto restore_grace = 18s;

    struct client_state_t {
      std::uint64_t generation = 0;
      bool active = false;
      bool reconnect_generation = false;
      bool reconnect_pending = false;
      bool startup_apply_queued = false;
      bool restore_pending = false;
      bool apply_remote_on_connect = false;
      bool restore_desk_on_disconnect = false;
    };

    std::mutex state_mutex;
    std::unordered_map<std::string, client_state_t> client_states;

    std::string_view profile_name(profile_e profile) {
      switch (profile) {
        case profile_e::desk:
          return "desk";
        case profile_e::remote:
          return "remote";
        case profile_e::tv:
          return "tv";
      }
      return {};
    }

    std::size_t discard_response(char *, std::size_t size, std::size_t count, void *) {
      return size * count;
    }

    status_e apply(profile_e profile) {
      const auto name = profile_name(profile);
      if (name.empty()) {
        return status_e::failed;
      }

      CURL *curl = curl_easy_init();
      if (curl == nullptr) {
        BOOST_LOG(error) << "Remote display: unable to initialize CURL";
        return status_e::unavailable;
      }

      const std::string url =
        "http://127.0.0.1:3780/api/v1/profiles/" + std::string(name) + "/apply";
      constexpr char request_body[] = R"({"applyMode":"strict_then_compatible"})";
      curl_slist *headers = nullptr;
      headers = curl_slist_append(headers, "Content-Type: application/json");

      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, sizeof(request_body) - 1);
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 750L);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 15000L);
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_response);

      const CURLcode result = curl_easy_perform(curl);
      long response_code = 0;
      if (result == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      }

      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);

      if (result == CURLE_COULDNT_CONNECT || result == CURLE_OPERATION_TIMEDOUT) {
        BOOST_LOG(warning) << "Remote display controller is unavailable";
        return status_e::unavailable;
      }
      if (result != CURLE_OK || response_code < 200 || response_code >= 300) {
        BOOST_LOG(error) << "Remote display profile [" << name
                         << "] failed (curl " << static_cast<int>(result)
                         << ", HTTP " << response_code << ')';
        return status_e::failed;
      }

      BOOST_LOG(info) << "Remote display profile [" << name << "] applied";
      return status_e::applied;
    }

    bool is_current_active(const std::string &client_id, std::uint64_t generation) {
      std::lock_guard lock(state_mutex);
      auto pos = client_states.find(client_id);
      return pos != client_states.end() &&
             pos->second.generation == generation &&
             pos->second.active;
    }

    void apply_startup_if_current(const std::string &client_id, std::uint64_t generation) {
      if (is_current_active(client_id, generation)) {
        apply(profile_e::remote);
      }
    }

    void clear_reconnect_if_current(const std::string &client_id, std::uint64_t generation) {
      std::lock_guard lock(state_mutex);
      auto pos = client_states.find(client_id);
      if (pos != client_states.end() &&
          pos->second.generation == generation &&
          !pos->second.active) {
        pos->second.reconnect_pending = false;
      }
    }

    void restore_if_current(const std::string &client_id, std::uint64_t generation) {
      {
        std::lock_guard lock(state_mutex);
        auto pos = client_states.find(client_id);
        if (pos == client_states.end() ||
            pos->second.generation != generation ||
            pos->second.active ||
            !pos->second.restore_pending ||
            !pos->second.restore_desk_on_disconnect) {
          return;
        }
        pos->second.restore_pending = false;
      }

      apply(profile_e::desk);
    }
  }  // namespace

  void session_connected(
    const std::string &client_id,
    std::uint64_t generation
  ) {
    if (generation == 0) {
      return;
    }

    std::lock_guard lock(state_mutex);
    auto &state = client_states[client_id];
    state.reconnect_generation = state.active || state.reconnect_pending;
    state.generation = generation;
    state.reconnect_pending = false;
    state.restore_pending = false;
    state.startup_apply_queued = false;
    state.active = true;
  }

  void set_policy(
    const std::string &client_id,
    std::uint64_t generation,
    bool apply_remote_on_connect,
    bool restore_desk_on_disconnect
  ) {
    bool queue_startup_apply = false;
    {
      std::lock_guard lock(state_mutex);
      auto pos = client_states.find(client_id);
      if (pos == client_states.end() ||
          pos->second.generation != generation ||
          !pos->second.active) {
        return;
      }

      auto &state = pos->second;
      state.apply_remote_on_connect = apply_remote_on_connect;
      state.restore_desk_on_disconnect = restore_desk_on_disconnect;
      if (apply_remote_on_connect &&
          !state.reconnect_generation &&
          !state.startup_apply_queued) {
        state.startup_apply_queued = true;
        queue_startup_apply = true;
      }
    }

    if (queue_startup_apply) {
      task_pool.push(apply_startup_if_current, client_id, generation);
    }
  }

  status_e apply_for_session(
    const std::string &client_id,
    std::uint64_t generation,
    profile_e profile
  ) {
    if (!is_current_active(client_id, generation)) {
      return status_e::failed;
    }
    return apply(profile);
  }

  void session_disconnected(
    const std::string &client_id,
    std::uint64_t generation,
    bool intentional
  ) {
    bool schedule_restore = false;
    {
      std::lock_guard lock(state_mutex);
      auto pos = client_states.find(client_id);
      if (pos == client_states.end() ||
          pos->second.generation != generation) {
        return;
      }

      auto &state = pos->second;
      state.active = false;
      state.reconnect_pending = !intentional;
      if (state.restore_desk_on_disconnect) {
        state.restore_pending = true;
        schedule_restore = true;
      }
    }

    if (!schedule_restore) {
      if (!intentional) {
        task_pool.pushDelayed(
          clear_reconnect_if_current,
          restore_grace,
          client_id,
          generation
        );
      }
      return;
    }

    if (intentional) {
      task_pool.push(restore_if_current, client_id, generation);
    }
    else {
      task_pool.pushDelayed(
        clear_reconnect_if_current,
        restore_grace,
        client_id,
        generation
      );
      task_pool.pushDelayed(
        restore_if_current,
        restore_grace,
        client_id,
        generation
      );
    }
  }
}  // namespace remote_display
