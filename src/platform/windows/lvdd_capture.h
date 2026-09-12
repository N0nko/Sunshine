/** @file src/platform/windows/lvdd_capture.h
 * @brief Optional on-demand GPU mailbox consumer for the custom LVDD driver.
 */
#pragma once
#include "src/platform/common.h"

#include <chrono>
#include <d3d11.h>
#include <memory>
#include <string>

namespace platf::dxgi {
  class display_base_t;

  /** @brief SYSTEM-only direct capture; ownership and GPU synchronization are private. */
  class lvdd_capture_t {
    struct impl;
    std::unique_ptr<impl> state;

  public:
    lvdd_capture_t();
    ~lvdd_capture_t();
    /** @brief Avoid repeatedly reopening a failed producer during capture recovery. */
    static bool available();
    /** @brief Switch to DXGI for a requirement the direct surface cannot satisfy. */
    static capture_e fallback();
    /** @brief Attach the exact display target, validate color space and import GPU surfaces. */
    int init(display_base_t *display, const std::string &name);
    /** @brief Acquire newest available texture, bounded by the caller's timeout. */
    capture_e next_frame(std::chrono::milliseconds timeout, ID3D11Texture2D **out, uint64_t &qpc);
    /** @brief Return the GPU slot after the consumer's copy has been submitted. */
    capture_e release_frame();
  };
}  // namespace platf::dxgi
