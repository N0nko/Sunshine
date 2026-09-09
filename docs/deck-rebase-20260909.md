# Deck Sunshine rebase: 2026-09-09

## Source and rollback

- Previous installed source: `a69748a715d55f7796133114fe70eed8692d55f1`.
- Previous upstream base: `cf52f4b`.
- New upstream: `v2026.906.222525` (`cb72dffa3233c5815cd5ba88f09f049dd679ba75`).
- Candidate branch: `deck/rebase-2026.906`.
- The old branch and installed package are not modified by the rebase.

## Preserved patch stack

All 15 commits were replayed, retaining their individual boundaries:

1. Warm-resume identity cache.
2. Guarded RTSP request replacement.
3. Guarded fast resume.
4. Focused Windows AMD64 validation workflow.
5. Acknowledged live bitrate control.
6. NVENC bitrate test double.
7. Generation-safe remote display bridge.
8. Native Deck microphone sink.
9. Generation-safe Vulkan HDR stream state publisher.
10. Restack validation notes.
11. Independent Deck extension contract tests.
12. Optional minimum-FPS deadline pacing.
13. WGC excluded from the pacing scope.
14. Frame-event deadline wait.
15. Avoid redundant DXGI capture pacing at the display rate.

Conflicts were confined to stream member declarations, includes and teardown.
The resolution preserves the new upstream input session identity and controller
termination, alongside the Deck remote-display restore policy. Upstream's new
libvirtualhid backend and input validation are not replaced by the old backend.

## Controller disconnect policy

Upstream intentionally retains virtual controllers across paused streams.
`retain_gamepads_on_disconnect` defaults to enabled to preserve that behavior.
For the Deck host, select disabled (Inputs: Keep Controllers When Disconnected).

Disabled creates an independent input context for each connection and destroys
its controllers on session teardown, on the existing serialized input task queue.
No polling, additional thread, reset delay or driver restart is introduced.
A replacement connection never shares its retiring context; duplicate teardown
cannot release a newly reused controller slot. Other clients' controllers are
not destroyed. Temporary network recovery inside the same session is unchanged.

This does not shorten transport disconnect detection. Games that do not support
controller hotplug may require reselection after recreation. A new physical
controller may take a freed slot; a resumed Deck cannot also retain that slot.

## Validation gate

The Windows workflow builds the app and web assets, runs Deck protocol/warm-resume
contracts and input/configuration/security/NVENC tests, checks runtime DLL linkage,
and packages a ZIP with SHA-256. The input test covers retention, release,
overlapping reconnect contexts, slot reuse, duplicate teardown and other clients.

CI/fake-backend tests cannot certify physical controller enumeration, the paid
driver, real hibernation, HDR output, microphone audio or end-to-end pacing.
These require host/Deck smoke tests after a verified package is installed.
Do not buy/activate a license automatically or delete the existing ViGEm fallback.
