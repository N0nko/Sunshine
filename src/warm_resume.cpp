/**
 * @file src/warm_resume.cpp
 * @brief Guarded identity cache for fast stream replacement.
 */
#include "warm_resume.h"

#include <utility>

namespace warm_resume {
  bool fingerprint_t::valid() const {
    return appid > 0 &&
           width > 0 &&
           height > 0 &&
           fps > 0 &&
           !client_cert.empty();
  }

  bool fingerprint_t::matches(const fingerprint_t &other) const {
    return valid() &&
           other.valid() &&
           appid == other.appid &&
           width == other.width &&
           height == other.height &&
           fps == other.fps &&
           enable_hdr == other.enable_hdr &&
           enable_sops == other.enable_sops &&
           client_cert == other.client_cert;
  }

  bool cache_t::matches(const fingerprint_t &fingerprint) const {
    std::lock_guard lock {mutex_};
    return fingerprint_.has_value() && fingerprint_->matches(fingerprint);
  }

  void cache_t::remember(fingerprint_t fingerprint) {
    std::lock_guard lock {mutex_};
    if (fingerprint.valid()) {
      fingerprint_ = std::move(fingerprint);
    } else {
      fingerprint_.reset();
    }
  }

  void cache_t::clear() {
    std::lock_guard lock {mutex_};
    fingerprint_.reset();
  }
}  // namespace warm_resume
