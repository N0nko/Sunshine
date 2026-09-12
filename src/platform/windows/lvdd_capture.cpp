/** @file src/platform/windows/lvdd_capture.cpp
 * @brief Import IddCx surfaces without Desktop Duplication or CPU pixel readback.
 */
#include "lvdd_capture.h"

#include "display.h"
#include "lvdd_capture_protocol.h"
#include "src/logging.h"
#include "utf_utils.h"

#include <algorithm>
#include <setupapi.h>
#include <thread>
#include <wrl/client.h>

namespace platf::dxgi {
  using Microsoft::WRL::ComPtr;

  namespace {
    std::atomic<long long> retry_after {0};  ///< Monotonic backoff after driver/mode loss.

    /** @brief Keep recovery on the established DXGI path for thirty seconds. */
    capture_e lost() {
      retry_after = (std::chrono::steady_clock::now() + std::chrono::seconds(30)).time_since_epoch().count();
      return capture_e::reinit;
    }

    constexpr GUID control_guid {0x5f894d6c, 0x3a69, 0x48a2, {0x86, 0xef, 0xe4, 0xc6, 0x71, 0x93, 0x2d, 0x63}};

    /** @brief Small move-only NT handle owner. */
    struct handle {
      HANDLE value {};
      handle() = default;
      handle(const handle &) = delete;
      handle &operator=(const handle &) = delete;

      ~handle() {
        reset();
      }

      void reset(HANDLE h = nullptr) {
        if (value && value != INVALID_HANDLE_VALUE) {
          CloseHandle(value);
        }
        value = h;
      }
    };

    /** @brief Match both target ID and adapter LUID, never a coincidentally equal physical target ID. */
    bool target_for(const std::string &name, lvdd_capture::request &request) {
      UINT32 path_count, mode_count;
      if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count)) {
        return false;
      }
      std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
      std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
      if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr)) {
        return false;
      }
      for (UINT32 i = 0; i < path_count; ++i) {
        const auto &path = paths[i];
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source {};
        source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source), path.sourceInfo.adapterId, path.sourceInfo.id};
        if (!DisplayConfigGetDeviceInfo(&source.header) && utf_utils::to_utf8(source.viewGdiDeviceName) == name) {
          request.target_id = path.targetInfo.id;
          request.adapter_low = path.targetInfo.adapterId.LowPart;
          request.adapter_high = path.targetInfo.adapterId.HighPart;
          return true;
        }
      }
      return false;
    }

    /** @brief Open the existing restricted driver interface; never widen its ACL. */
    HANDLE open_driver() {
      auto devices = SetupDiGetClassDevsW(&control_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
      if (devices == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
      }
      HANDLE result = INVALID_HANDLE_VALUE;
      for (DWORD i = 0;; ++i) {
        SP_DEVICE_INTERFACE_DATA iface {sizeof(iface)};
        if (!SetupDiEnumDeviceInterfaces(devices, nullptr, &control_guid, i, &iface)) {
          break;
        }
        DWORD bytes = 0;
        SetupDiGetDeviceInterfaceDetailW(devices, &iface, nullptr, 0, &bytes, nullptr);
        if (bytes < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) {
          continue;
        }
        std::vector<BYTE> buffer(bytes);
        auto detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data());
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(devices, &iface, detail, bytes, nullptr, nullptr)) {
          continue;
        }
        result = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (result != INVALID_HANDLE_VALUE) {
          break;
        }
      }
      SetupDiDestroyDeviceInfoList(devices);
      return result;
    }
  }  // namespace

  /** @brief Pinned producer generation plus one acquired keyed mutex. */
  struct lvdd_capture_t::impl {
    handle file, process, mapping, lock, event, textures[lvdd_capture::slots];
    lvdd_capture::shared *metadata {};
    ComPtr<ID3D11Texture2D> images[lvdd_capture::slots];
    ComPtr<IDXGIKeyedMutex> keyed[lvdd_capture::slots];
    ComPtr<ID3D11DeviceContext> context;
    std::uint64_t last_sequence {}, selected {}, skipped {};
    int held {-1};

    ~impl() {
      release();
      if (metadata) {
        UnmapViewOfFile(metadata);
      }
      if (selected) {
        BOOST_LOG(info) << "LVDD direct capture: consumed=" << selected << " mailbox_skipped=" << skipped;
      }
    }

    /** @brief Complete producer/consumer GPU ownership transfer. */
    capture_e release() {
      if (held < 0) {
        return capture_e::ok;
      }
      context->Flush();
      const auto hr = keyed[held]->ReleaseSync(0);
      held = -1;
      return hr == S_OK ? capture_e::ok : capture_e::reinit;
    }

    /** @brief Duplicate exactly one producer handle while the WDF file pins its resource. */
    bool duplicate(std::uint64_t source, handle &target) {
      return source && DuplicateHandle(process.value, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(source)), GetCurrentProcess(), &target.value, 0, FALSE, DUPLICATE_SAME_ACCESS);
    }
  };

  lvdd_capture_t::lvdd_capture_t():
      state(std::make_unique<impl>()) {}

  lvdd_capture_t::~lvdd_capture_t() = default;

  bool lvdd_capture_t::available() {
    return std::chrono::steady_clock::now().time_since_epoch().count() >= retry_after.load();
  }

  int lvdd_capture_t::init(display_base_t *display, const std::string &name) {
    auto failure = util::fail_guard([]() {
      lost();
    });
    lvdd_capture::request request;
    if (!target_for(name, request) || display->display_rotation > DXGI_MODE_ROTATION_IDENTITY) {
      return -1;
    }
    auto &s = *state;
    s.file.reset(open_driver());
    if (s.file.value == INVALID_HANDLE_VALUE) {
      BOOST_LOG(warning) << "LVDD direct capture unavailable: driver interface error " << GetLastError();
      return -1;
    }
    lvdd_capture::reply reply;
    DWORD returned = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!DeviceIoControl(s.file.value, lvdd_capture::attach_ioctl, &request, sizeof(request), &reply, sizeof(reply), &returned, nullptr)) {
      const auto error = GetLastError();
      if (error != ERROR_NOT_READY || std::chrono::steady_clock::now() >= deadline) {
        BOOST_LOG(warning) << "LVDD direct capture attach failed: " << error;
        return -1;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (returned != sizeof(reply) || reply.size != sizeof(reply) || reply.protocol != lvdd_capture::version || reply.mapping_bytes != sizeof(lvdd_capture::shared) || !reply.producer_pid) {
      return -1;
    }
    s.process.reset(OpenProcess(PROCESS_DUP_HANDLE | SYNCHRONIZE, FALSE, reply.producer_pid));
    if (!s.process.value || !s.duplicate(reply.mapping, s.mapping) || !s.duplicate(reply.lock, s.lock) || !s.duplicate(reply.event, s.event)) {
      BOOST_LOG(warning) << "LVDD direct capture handle import failed: " << GetLastError();
      return -1;
    }
    s.metadata = static_cast<lvdd_capture::shared *>(MapViewOfFile(s.mapping.value, FILE_MAP_READ, 0, 0, sizeof(lvdd_capture::shared)));
    if (!s.metadata || s.metadata->size != sizeof(*s.metadata) || s.metadata->protocol != lvdd_capture::version) {
      return -1;
    }
    const auto &m = *s.metadata;
    DXGI_ADAPTER_DESC1 adapter {};
    display->adapter->GetDesc1(&adapter);
    const bool scrgb = m.format == DXGI_FORMAT_R16G16B16A16_FLOAT && m.color_space == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
    const bool sdr = (m.format == DXGI_FORMAT_B8G8R8A8_UNORM || m.format == DXGI_FORMAT_R8G8B8A8_UNORM) &&
                     m.color_space == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    BOOST_LOG(info) << "LVDD direct surface: " << m.width << 'x' << m.height << " format=" << m.format << " color_space=" << m.color_space << " SDR_white=" << m.sdr_white_nits;
    if (m.width != static_cast<unsigned>(display->width_before_rotation) || m.height != static_cast<unsigned>(display->height_before_rotation) || m.adapter_low != adapter.AdapterLuid.LowPart || m.adapter_high != adapter.AdapterLuid.HighPart || (!scrgb && !sdr) || (display->is_hdr() && !scrgb)) {
      BOOST_LOG(warning) << "LVDD surface incompatible with existing encoder color pipeline; using DXGI instead of altering HDR math";
      return -1;
    }
    ComPtr<ID3D11Device1> device1;
    if (FAILED(display->device->QueryInterface(IID_PPV_ARGS(&device1)))) {
      return -1;
    }
    display->device->GetImmediateContext(&s.context);
    for (unsigned i = 0; i < lvdd_capture::slots; ++i) {
      if (!s.duplicate(reply.textures[i], s.textures[i]) || FAILED(device1->OpenSharedResource1(s.textures[i].value, IID_PPV_ARGS(&s.images[i]))) || FAILED(s.images[i].As(&s.keyed[i]))) {
        return -1;
      }
      D3D11_TEXTURE2D_DESC desc {};
      s.images[i]->GetDesc(&desc);
      if (desc.Width != m.width || desc.Height != m.height || desc.Format != m.format || desc.ArraySize != 1 || desc.SampleDesc.Count != 1) {
        return -1;
      }
    }
    display->capture_format = static_cast<DXGI_FORMAT>(m.format);
    BOOST_LOG(info) << "Capture backend: LVDD direct GPU mailbox (IddCx -> shared D3D11 -> encoder; no CPU readback)";
    failure.disable();
    return 0;
  }

  capture_e lvdd_capture_t::next_frame(std::chrono::milliseconds timeout, ID3D11Texture2D **out, uint64_t &qpc) {
    auto &s = *state;
    if (s.release() != capture_e::ok) {
      return lost();
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
      if (s.metadata->state != 1 || WaitForSingleObject(s.process.value, 0) == WAIT_OBJECT_0) {
        return lost();
      }
      const auto lock_result = WaitForSingleObject(s.lock.value, 0);
      if (lock_result == WAIT_ABANDONED) {
        ReleaseMutex(s.lock.value);
        return lost();
      }
      if (lock_result == WAIT_OBJECT_0) {
        const unsigned latest = lvdd_capture::latest(s.metadata->frames);
        const auto frame = s.metadata->frames[latest];
        if (frame.sequence > s.last_sequence && s.keyed[latest]->AcquireSync(0, 0) == S_OK) {
          s.held = latest;
          if (s.last_sequence) {
            s.skipped += frame.sequence - s.last_sequence - 1;
          }
          s.last_sequence = frame.sequence;
          ++s.selected;
          qpc = frame.present_qpc ? frame.present_qpc : frame.acquire_qpc;
          s.images[latest].CopyTo(out);
          ReleaseMutex(s.lock.value);
          return capture_e::ok;
        }
        ReleaseMutex(s.lock.value);
      }
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
      if (remaining <= 0) {
        return capture_e::timeout;
      }
      HANDLE events[] {s.event.value, s.process.value};
      const auto wait = WaitForMultipleObjects(2, events, FALSE, static_cast<DWORD>(remaining));
      if (wait == WAIT_TIMEOUT) {
        return capture_e::timeout;
      }
      if (wait != WAIT_OBJECT_0) {
        return lost();
      }
    }
  }

  capture_e lvdd_capture_t::release_frame() {
    return state->release();
  }
}  // namespace platf::dxgi
