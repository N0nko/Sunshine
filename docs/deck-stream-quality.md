# Deck stream quality candidate

Baseline: fea335a57f96e9e4bcfed56db758fee91d6c2aec.

## Implemented

`adaptive_packet_pacing = enabled` enables bounded bitrate-aware UDP batches.
The Advanced settings page exposes the switch. Default remains disabled.
The last successful live bitrate result updates the sender's allowance; failed
requests do not. Wire-size estimates include FEC and framing headroom.

There is no extra frame buffer, no idle timer and no new protocol or port.
The scheduler requests at most 2 ms or one quarter frame of intentional delay,
whichever is smaller. Actual timer wakeups and network sends may take longer.
This is not a bandwidth limiter: large keyframes can still burst after the cap.

## Validation

`test_deck_extensions` covers schedule bounds, rates, FEC, packet sizes, extreme
inputs and independent frames, alongside existing protocol/resume contracts.
Full Windows builds must also run existing input/config/NVENC regressions.

For live A/B testing, hold resolution, source rate, codec, bitrate, HDR, GPU
priority and client pacing constant. Compare frame send span, packet loss,
arrival/decode timing, presentation misses and power, not only average FPS.
Use the existing debug network timing loggers. A 2 ms requested cap is not a
claim of 2 ms total network latency or photon-to-photon latency.

## Not implemented in this candidate

Direct LVDD surface capture and cross-host A/V clock synchronization remain
separate work. Neither is silently substituted with this transport feature.
Punktfunk's current in-driver encoder is not imported. A direct-capture driver
must validate cursor, per-frame pixel/color-space metadata, HDR transitions,
access control, non-blocking surface ownership, teardown and signed packaging
before it can replace the working DXGI path.

## Rollback

Disable the option and restart Sunshine to restore the original packet sender.
The existing signed display driver, NVENC settings, microphone, touch and
controller backends are unchanged. Do not install a new display driver or
change GPU priority/HAGS as part of this candidate.
