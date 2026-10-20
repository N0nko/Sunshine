/**
 * @file src/stream_ping.h
 * @brief Session-owned queues for interruptible initial audio/video handshakes.
 */
#pragma once

#include "thread_safe.h"

#include <memory>

namespace stream {
  /**
   * @brief Keep ping queues alive before workers start, so stop cannot miss a waiter.
   * @tparam Packet Endpoint and payload delivered by the UDP receiver.
   */
  template<class Packet>
  struct startup_ping_queues_t {
    const std::shared_ptr<safe::queue_t<Packet>> video = std::make_shared<safe::queue_t<Packet>>(30);  ///< Video handshake queue.
    const std::shared_ptr<safe::queue_t<Packet>> audio = std::make_shared<safe::queue_t<Packet>>(30);  ///< Audio handshake queue.

    /**
     * @brief Wake both handshake workers immediately, including workers that start after cancellation.
     */
    void stop() {
      video->stop();
      audio->stop();
    }
  };
}  // namespace stream
