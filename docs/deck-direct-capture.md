# Optional LVDD direct GPU capture

Select `LVDD Direct GPU (experimental, DXGI fallback)` in Advanced / Capture,
or set `capture = lvdd`. The installed custom driver must implement the matching
private `lvdd_capture_protocol.h` contract. Normal/automatic capture is unchanged.

Path: IddCx desktop surface -> shared D3D11 mailbox -> existing Sunshine image
pool -> existing color conversion/NVENC. This removes Desktop Duplication from
frame acquisition, not DWM composition. There is no CPU pixel readback, but there
are GPU copies: this is not a claim of literal zero-copy or application hooking.
It does not depend on WGC or replace the audio capture implementation.

The three resources are an overwriteable mailbox, not three buffered frames.
Sunshine selects the newest sequence and never waits for an older entry. The
producer never waits for Sunshine. Device/mode loss closes a generation; a short
backoff makes recovery use DXGI rather than repeatedly reopening a failed bridge.

The capture interface retains its SYSTEM/broker-only ACL. Unnamed handles are
pinned by a WDF file and duplicated from the driver host. No network endpoint,
new service, broad ACL, injection, or driver-signature policy change is required.
Portable unelevated Sunshine falls back. Adapter LUID and OS target ID must both
match. Software encoders, rotated outputs and unsupported color spaces fall back.

HDR accepts only FP16 linear scRGB, retaining the existing encoder HDR math.
SDR accepts normal RGBA/BGRA sRGB. No black clamp, range guess or new tone curve is
added. This driver's existing software cursor is part of the source surface;
requesting per-client cursor suppression switches to DXGI.

For measurement, use the producer handoff QPC, not IddCx's scheduled presentation
deadline (which could misleadingly look perfectly paced). Host latency starts at
that handoff, after the producer copy submission; it is not input-to-photon latency.
Compare client presentation gaps, packet arrival and decode under identical
codec/resolution/refresh/bitrate conditions. A short pendulum run is not enough to
prove long-session, hibernation, or every-game reliability. Record actual backend
log lines so fallback is never counted as a direct-capture result.

Rollback: set `capture = ddx`, restart Sunshine, and reconnect. The old driver
package must be exported before deployment. Driver rollback must target only the
candidate's INF, retaining the original trusted driver, custom modes and HDR layer.
