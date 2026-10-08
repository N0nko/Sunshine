# Sunshine stable rebase: 2026-10-09

## Baseline and target

- Installed source (read-only installed log): `58624f226cf639f5af18acee39bc91899bb8eae0`, version `2026.906.222525`.
- Previous upstream base: `cb72dffa3233c5815cd5ba88f09f049dd679ba75`.
- Source branch: `deck/direct-capture-final-20260912`, tip `42a0e520eacb57fe96563eda3f9f9b4e8e4626ec` (LVDD patch/license archive after the installed source).
- Official latest non-prerelease: `v2026.914.233613`, SHA `63d35f702ee9e362e43263742981836ec0710384`.
- Official API: https://api.github.com/repos/LizardByte/Sunshine/releases/latest
- Release: https://github.com/LizardByte/Sunshine/releases/tag/v2026.914.233613
- October 8 releases were marked prerelease and were deliberately not selected.
- Candidate: `deck/rebase-stable-20261009`, fork https://github.com/N0nko/Sunshine.
- Isolated checkout: `C:/Projects/Home/tmp/sunshine-stable-20261009`.
- Managed worktree creation returned `Not a git repository` for the chat root; a separate local clone was used instead. No existing worktree was repurposed.

## Preserved extensions and resolutions

All 23 source commits were replayed, with no skipped feature commits. The microphone, display bridge, HDR publisher, LVDD capture, protocol, warm-resume, and packet-pacing helper files are unchanged from the original fork tip.

- Native touch/pen and upstream logical-coordinate fixes are retained.
- Encrypted DeckMic configuration/Opus transport, status replies, generation ownership, and sink lifecycle are retained.
- Acknowledged live bitrate and allowlisted display controls are retained.
- Global/local `SunshineVirtualHdrActive` events and shutdown-safe publisher teardown are retained.
- Direct LVDD mailbox capture remains opt-in with DXGI fallback; WGC remains outside deadline pacing scope.
- Exact deadline pacing and bounded adaptive packet pacing remain opt-in, disabled by default.
- Per-connection controller release, stale-disconnect protection, retained-session policy, fast-resume identity checks, and display lifecycle policy are retained.
- No GPU-priority adjustment or experimental default was introduced.

Conflicts were limited to `Inputs.vue`, `en.json`, and `test_thread_safe.cpp`. Both the new upstream gamepad-driver selector and custom disconnect checkbox remain. Upstream category translations remain. The obsolete custom WGC warning is still removed. The three upstream queue overflow/stop tests coexist with the two custom absolute-deadline event tests.

## Companion dependencies

The candidate adopts stable upstream pins, not floating master:

- build-deps: `a1fe2841cbc0d8c4501a1006d2d1cb88f219cc8c`.
- doxyconfig: `419127bad87f49b2d45fa957ea7302abbb49c01f`.
- libvirtualhid: `53e1a949fc0784af716b782ddfa6c647cafd1f05`.
- Sunshine-local moonlight-common-c: `62e066388f1a1b133e0bee947b9a374311a3354b`.
- plasma-wayland-protocols: `382dfabda886d3f2f5c067b22e5a22376685ba78`.

No shared Moonlight checkout or companion working tree outside this Sunshine checkout was changed. Upstream raises the separately installed Virtual HID Driver minimum to `2026.914.1218.10`; native device behavior still requires a later authorized hardware smoke test. ViGEm fallback remains available.

## Validation

- Local MSYS2 UCRT64 compile/run: 20 DeckProtocol, WarmResumeCache, PacketPacing, and LvddCapture tests passed.
- Local MSYS2 UCRT64 `-Wall -Werror` compile/run: all 5 merged ThreadSafeQueue/ThreadSafeEvent tests passed.
- `git diff --check` and actionlint passed.
- Windows CI: https://github.com/N0nko/Sunshine/actions/runs/37848931287
- Build revision: `5426ec12b96d6196d86d82fefa84162dc58314f9`.
- CI status at initial report: in progress; full compilation, regression tests, linkage and packaging not yet certified.
- CI expands the existing input/pairing/crypto/config/NVENC filter with thread-safe, Virtual HID, bind-address and mDNS tests.

Nothing was installed, restarted, or changed in device/network/power settings. Original dirty `docs/deck-stream-quality.md` remains in the source checkout, uncommitted and not included here. Source branch and installed package remain rollback options.

## Integration

Integrate the candidate branch as a whole, not just its final CI commit: the 23 rebased feature commits are based on the new upstream SHA. Do not force-push or overwrite the original branch. `git range-diff cb72dffa..42a0e52 v2026.914.233613..3e13dbd3` records semantic replay differences. Review the following complete custom-file manifest against stable upstream.

- `.github/workflows/deck-windows.yml`
- `cmake/compile_definitions/common.cmake`
- `cmake/compile_definitions/windows.cmake`
- `docs/configuration.md`
- `docs/deck-direct-capture.md`
- `docs/deck-rebase-20260909.md`
- `docs/deck-stream-quality.md`
- `docs/patches/libvirtualdisplay-LICENSE.txt`
- `docs/patches/lvdd-direct-capture-v1.6.3.patch`
- `src/config.cpp`
- `src/config.h`
- `src/deck_protocol.h`
- `src/globals.h`
- `src/input.cpp`
- `src/input.h`
- `src/nvenc/nvenc_base.cpp`
- `src/nvenc/nvenc_base.h`
- `src/nvenc/nvenc_encoder.h`
- `src/nvhttp.cpp`
- `src/packet_pacing.h`
- `src/platform/windows/deck_microphone.cpp`
- `src/platform/windows/deck_microphone.h`
- `src/platform/windows/display.h`
- `src/platform/windows/display_base.cpp`
- `src/platform/windows/display_vram.cpp`
- `src/platform/windows/lvdd_capture.cpp`
- `src/platform/windows/lvdd_capture.h`
- `src/platform/windows/lvdd_capture_protocol.h`
- `src/platform/windows/vulkan_hdr_state.cpp`
- `src/platform/windows/vulkan_hdr_state.h`
- `src/remote_display.cpp`
- `src/remote_display.h`
- `src/rtsp.cpp`
- `src/rtsp.h`
- `src/stream.cpp`
- `src/thread_safe.h`
- `src/video.cpp`
- `src/video.h`
- `src/warm_resume.cpp`
- `src/warm_resume.h`
- `src_assets/common/assets/web/config.html`
- `src_assets/common/assets/web/configs/tabs/Advanced.vue`
- `src_assets/common/assets/web/configs/tabs/Inputs.vue`
- `src_assets/common/assets/web/configs/tabs/audiovideo/DisplayModesSettings.vue`
- `src_assets/common/assets/web/public/assets/locale/en.json`
- `tests/CMakeLists.txt`
- `tests/unit/test_deck_protocol.cpp`
- `tests/unit/test_input.cpp`
- `tests/unit/test_lvdd_capture.cpp`
- `tests/unit/test_nvenc_dynamic_factory.cpp`
- `tests/unit/test_packet_pacing.cpp`
- `tests/unit/test_thread_safe.cpp`
- `tests/unit/test_warm_resume.cpp`

## Replay Audit

```text
 1:  e49aa797 =  1:  5c2dcaa2 Add guarded warm resume identity cache
 2:  6b2cbccb =  2:  20680590 Allow guarded RTSP request replacement
 3:  748e19a6 =  3:  61817524 Add guarded Sunshine fast resume
 4:  900f4a31 =  4:  1e2364e3 Add focused Windows AMD64 validation
 5:  e1432644 =  5:  4d766110 Add acknowledged live bitrate control
 6:  02a21682 =  6:  e42b1495 Fix NVENC test double bitrate override
 7:  ab394fc8 =  7:  fc5b613b Add generation-safe remote display bridge
 8:  a0fcdeb9 =  8:  84207598 Add native Deck microphone sink
 9:  1d159858 =  9:  d1af7139 Publish generation-safe Vulkan HDR stream state
10:  3d5e8272 = 10:  c32254ee Validate clean Sunshine restack
11:  19266e49 = 11:  4547ee9c Isolate Deck extension contract tests
12:  784bff49 ! 12:  cbf71253 Add exact minimum-FPS deadline pacing
    @@ Commit message
         Add exact minimum-FPS deadline pacing
     
      ## docs/configuration.md ##
    -@@ docs/configuration.md: editing the `conf` file in a text editor. Use the examples as reference.
    +@@ docs/configuration.md: supported on the current platform.
          </tr>
      </table>
      
    @@ src_assets/common/assets/web/configs/tabs/audiovideo/DisplayModesSettings.vue: c
     
      ## src_assets/common/assets/web/public/assets/locale/en.json ##
     @@
    -     "bind_address_desc": "Set the specific IP address Sunshine will bind to. If left blank, Sunshine will bind to all available addresses.",
    -     "capture": "Force a Specific Capture Method",
    -     "capture_desc": "On automatic mode Sunshine will use the first one that works. NvFBC requires patched nvidia drivers.",
    +     "category_vaapi_encoder": "VA-API Encoder",
    +     "category_videotoolbox_encoder": "VideoToolbox Encoder",
    +     "category_vulkan_encoder": "Vulkan Encoder",
     +    "capture_wgc_portable_only": "Windows.Graphics.Capture is unavailable when Sunshine runs as a Windows service. Use the portable build for WGC, or select Desktop Duplication API for the installed service.",
          "cert": "Certificate",
          "cert_desc": "The certificate used for the web UI and Moonlight client pairing. For best compatibility, this should have an RSA-2048 public key.",
    @@ src_assets/common/assets/web/public/assets/locale/en.json
          "min_log_level_0": "Verbose",
          "min_log_level_1": "Debug",
     
    - ## tests/unit/test_thread_safe.cpp (new) ##
    -@@
    -+/**
    -+ * @file tests/unit/test_thread_safe.cpp
    -+ * @brief Test absolute-deadline queue waits.
    -+ */
    -+#include "../tests_common.h"
    -+
    -+#include <chrono>
    -+
    -+#include <src/thread_safe.h>
    -+
    -+using namespace std::chrono_literals;
    + ## tests/unit/test_thread_safe.cpp ##
    +@@ tests/unit/test_thread_safe.cpp: TEST(ThreadSafeQueue, RejectsItemsAfterStop) {
    + 
    +   EXPECT_FALSE(queue.raise(1));
    + }
     +
     +TEST(ThreadSafeQueueTest, PopUntilReturnsQueuedValue) {
     +  safe::queue_t<int> queue;
13:  8a7acb40 ! 13:  ec21424c Keep WGC outside pacing scope
    @@ src_assets/common/assets/web/configs/tabs/Advanced.vue: const config = ref(props
     
      ## src_assets/common/assets/web/public/assets/locale/en.json ##
     @@
    -     "bind_address_desc": "Set the specific IP address Sunshine will bind to. If left blank, Sunshine will bind to all available addresses.",
    -     "capture": "Force a Specific Capture Method",
    -     "capture_desc": "On automatic mode Sunshine will use the first one that works. NvFBC requires patched nvidia drivers.",
    +     "category_vaapi_encoder": "VA-API Encoder",
    +     "category_videotoolbox_encoder": "VideoToolbox Encoder",
    +     "category_vulkan_encoder": "Vulkan Encoder",
     -    "capture_wgc_portable_only": "Windows.Graphics.Capture is unavailable when Sunshine runs as a Windows service. Use the portable build for WGC, or select Desktop Duplication API for the installed service.",
          "cert": "Certificate",
          "cert_desc": "The certificate used for the web UI and Moonlight client pairing. For best compatibility, this should have an RSA-2048 public key.",
14:  b730c436 ! 14:  b5572a78 Put deadline wait on the frame event
    @@ src/thread_safe.h: namespace safe {
           *
     
      ## tests/unit/test_thread_safe.cpp ##
    -@@
    - 
    - using namespace std::chrono_literals;
    +@@ tests/unit/test_thread_safe.cpp: TEST(ThreadSafeQueue, RejectsItemsAfterStop) {
    +   EXPECT_FALSE(queue.raise(1));
    + }
      
     -TEST(ThreadSafeQueueTest, PopUntilReturnsQueuedValue) {
     -  safe::queue_t<int> queue;
15:  7af2a228 = 15:  d37f668e Avoid redundant pacing at display capture rate
16:  e4326727 ! 16:  24df1e03 Add optional per-connection controller release after upstream rebase
    @@ .github/workflows/deck-windows.yml: jobs:
                cd ..
     
      ## docs/configuration.md ##
    -@@ docs/configuration.md: editing the `conf` file in a text editor. Use the examples as reference.
    +@@ docs/configuration.md: supported on the current platform.
          </tr>
      </table>
      
    @@ src_assets/common/assets/web/config.html
                {
     
      ## src_assets/common/assets/web/configs/tabs/Inputs.vue ##
    -@@ src_assets/common/assets/web/configs/tabs/Inputs.vue: const config = ref(props.config)
    -               default="true"
    -     ></Checkbox>
    - 
    +@@ src_assets/common/assets/web/configs/tabs/Inputs.vue: watch(
    +       </select>
    +       <div class="form-text">{{ $t('config.gamepad_driver_desc') }}</div>
    +     </div>
     +    <Checkbox class="mb-3"
     +              id="retain_gamepads_on_disconnect"
     +              locale-prefix="config"
     +              v-model="config.retain_gamepads_on_disconnect"
     +              default="true"
     +    ></Checkbox>
    -+
    + 
          <!-- Emulated Gamepad Type -->
          <div class="mb-3" v-if="config.controller === 'enabled' && platform !== 'macos'">
    -       <label for="gamepad" class="form-label">{{ $t('config.gamepad') }}</label>
     
      ## src_assets/common/assets/web/public/assets/locale/en.json ##
     @@
17:  f4e3f541 = 17:  85e27285 Discard queued input test contexts before fixture teardown
18:  fea335a5 = 18:  6effeb9a Avoid HDR publisher access after static logging teardown
19:  232789d2 ! 19:  b79ed198 Add opt-in bounded bitrate-aware packet pacing
    @@ .github/workflows/deck-windows.yml: on:
        contents: read
     
      ## docs/configuration.md ##
    -@@ docs/configuration.md: editing the `conf` file in a text editor. Use the examples as reference.
    +@@ docs/configuration.md: supported on the current platform.
      
      ## Advanced
      
20:  cbf2cfea = 20:  04d504c6 Record inconclusive live packet comparison [skip ci]
21:  eef7be5e = 21:  8d5d907d Add opt-in LVDD direct GPU mailbox capture with DXGI fallback
22:  58624f22 = 22:  96e70bff Harden direct capture fallback and use measured handoff timestamps
23:  42a0e520 = 23:  3e13dbd3 Archive matching LVDD source patch and license [skip ci]
```
