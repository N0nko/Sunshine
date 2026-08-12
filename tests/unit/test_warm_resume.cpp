/**
 * @file tests/unit/test_warm_resume.cpp
 * @brief Tests for guarded warm-resume identity matching.
 */
#include <gtest/gtest.h>

#include "src/warm_resume.h"

namespace {
  warm_resume::fingerprint_t make_fingerprint() {
    return {
      .appid = 1,
      .width = 1280,
      .height = 800,
      .fps = 90,
      .enable_hdr = true,
      .enable_sops = false,
      .client_cert = "paired-client-certificate",
    };
  }
}  // namespace

TEST(WarmResumeCache, MatchesTheSameClientAndMode) {
  warm_resume::cache_t cache;
  const auto fingerprint = make_fingerprint();

  cache.remember(fingerprint);

  EXPECT_TRUE(cache.matches(fingerprint));
}

TEST(WarmResumeCache, RejectsChangedStreamIdentity) {
  warm_resume::cache_t cache;
  const auto fingerprint = make_fingerprint();
  cache.remember(fingerprint);

  auto changed_client = fingerprint;
  changed_client.client_cert = "different-certificate";
  EXPECT_FALSE(cache.matches(changed_client));

  auto changed_mode = fingerprint;
  changed_mode.fps = 60;
  EXPECT_FALSE(cache.matches(changed_mode));

  auto changed_app = fingerprint;
  changed_app.appid++;
  EXPECT_FALSE(cache.matches(changed_app));
}

TEST(WarmResumeCache, RejectsUnauthenticatedAndClearedState) {
  warm_resume::cache_t cache;
  auto fingerprint = make_fingerprint();
  fingerprint.client_cert.clear();

  cache.remember(fingerprint);
  EXPECT_FALSE(cache.matches(fingerprint));

  fingerprint = make_fingerprint();
  cache.remember(fingerprint);
  cache.clear();
  EXPECT_FALSE(cache.matches(fingerprint));
}
