/**
 * @file src/warm_resume.h
 * @brief Guarded identity cache for fast stream replacement.
 */
#pragma once

#include <mutex>
#include <optional>
#include <string>

namespace warm_resume {
  struct fingerprint_t {
    int appid;
    int width;
    int height;
    int fps;
    bool enable_hdr;
    bool enable_sops;
    std::string client_cert;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool matches(const fingerprint_t &other) const;
  };

  class cache_t {
  public:
    [[nodiscard]] bool matches(const fingerprint_t &fingerprint) const;
    void remember(fingerprint_t fingerprint);
    void clear();

  private:
    mutable std::mutex mutex_;
    std::optional<fingerprint_t> fingerprint_;
  };
}  // namespace warm_resume
