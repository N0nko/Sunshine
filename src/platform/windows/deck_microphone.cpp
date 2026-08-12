/**
 * @file src/platform/windows/deck_microphone.cpp
 * @brief Native Steam Deck microphone sink for Windows.
 */

#include "deck_microphone.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <Audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <opus/opus.h>
#include <propsys.h>

#include "src/logging.h"
#include "src/utility.h"

using namespace std::chrono_literals;

namespace platf::deck_microphone {
  namespace {
    constexpr std::uint32_t legacy_sample_rate = 16000;
    constexpr std::uint32_t opus_sample_rate = 48000;
    constexpr int opus_frame_samples = 960;
    constexpr std::uint32_t max_concealed_packets = 3;
    constexpr DWORD render_wait_ms = 25;
    constexpr PROPERTYKEY device_friendly_name_key = {
      {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
      14
    };

    template<class T>
    void release_com(T *value) {
      value->Release();
    }

    using device_enum_t = util::safe_ptr<IMMDeviceEnumerator, release_com<IMMDeviceEnumerator>>;
    using collection_t = util::safe_ptr<IMMDeviceCollection, release_com<IMMDeviceCollection>>;
    using device_t = util::safe_ptr<IMMDevice, release_com<IMMDevice>>;
    using property_store_t = util::safe_ptr<IPropertyStore, release_com<IPropertyStore>>;
    using audio_client_t = util::safe_ptr<IAudioClient, release_com<IAudioClient>>;
    using render_client_t = util::safe_ptr<IAudioRenderClient, release_com<IAudioRenderClient>>;

    struct close_handle_t {
      void operator()(void *value) const noexcept {
        if (value != nullptr && value != INVALID_HANDLE_VALUE) {
          CloseHandle(value);
        }
      }
    };

    using event_t = std::unique_ptr<void, close_handle_t>;

    class prop_variant_t {
    public:
      prop_variant_t() {
        PropVariantInit(&value);
      }

      ~prop_variant_t() {
        PropVariantClear(&value);
      }

      PROPVARIANT value;
    };

    struct stream_t {
      std::string owner;
      std::uint64_t generation;
      codec_e codec = codec_e::pcm_s16;
      std::uint32_t sample_rate = legacy_sample_rate;
      OpusDecoder *decoder = nullptr;
      std::mutex queue_mutex;
      std::deque<std::int16_t> samples;
      std::uint32_t last_sequence = 0;
      bool have_sequence = false;
      std::promise<status_e> ready;
      std::jthread thread;

      ~stream_t() {
        if (decoder != nullptr) {
          opus_decoder_destroy(decoder);
        }
      }
    };

    std::mutex service_mutex;
    std::shared_ptr<stream_t> active_stream;

    std::wstring lowercase(std::wstring text) {
      std::transform(text.begin(), text.end(), text.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
      });
      return text;
    }

    std::wstring endpoint_hint() {
      const char *override_value = std::getenv("SUNSHINE_DECK_MIC_SINK");
      if (override_value == nullptr || *override_value == '\0') {
        return L"CABLE Input";
      }
      std::string hint {override_value};
      return std::wstring(hint.begin(), hint.end());
    }

    device_t find_endpoint(device_enum_t &enumerator) {
      collection_t collection;
      HRESULT status = enumerator->EnumAudioEndpoints(
        eRender,
        DEVICE_STATE_ACTIVE,
        &collection
      );
      if (FAILED(status)) {
        BOOST_LOG(error) << "Deck microphone: endpoint enumeration failed";
        return nullptr;
      }

      UINT count = 0;
      collection->GetCount(&count);
      const std::wstring wanted = lowercase(endpoint_hint());
      for (UINT index = 0; index < count; ++index) {
        device_t device;
        if (FAILED(collection->Item(index, &device))) {
          continue;
        }

        property_store_t properties;
        if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) {
          continue;
        }

        prop_variant_t friendly_name;
        if (FAILED(properties->GetValue(device_friendly_name_key, &friendly_name.value)) ||
            friendly_name.value.vt != VT_LPWSTR ||
            friendly_name.value.pwszVal == nullptr) {
          continue;
        }

        const std::wstring name = lowercase(friendly_name.value.pwszVal);
        if (name.find(wanted) != std::wstring::npos) {
          return device;
        }
      }

      return nullptr;
    }

    status_e initialize_endpoint(
      const stream_t &stream,
      device_enum_t &enumerator,
      audio_client_t &audio_client,
      render_client_t &render_client,
      event_t &audio_event,
      UINT32 &buffer_frames
    ) {
      auto device = find_endpoint(enumerator);
      if (!device) {
        BOOST_LOG(warning) << "Deck microphone: CABLE Input endpoint not found";
        return status_e::unsupported;
      }

      HRESULT status = device->Activate(
        IID_IAudioClient,
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **>(&audio_client)
      );
      if (FAILED(status)) {
        BOOST_LOG(error) << "Deck microphone: unable to activate WASAPI";
        return status_e::failed;
      }

      WAVEFORMATEX format = {};
      format.wFormatTag = WAVE_FORMAT_PCM;
      format.nChannels = 1;
      format.nSamplesPerSec = stream.sample_rate;
      format.wBitsPerSample = 16;
      format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

      status = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
          AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY |
          AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
          AUDCLNT_STREAMFLAGS_NOPERSIST,
        0,
        0,
        &format,
        nullptr
      );
      if (FAILED(status)) {
        BOOST_LOG(error) << "Deck microphone: WASAPI format initialization failed";
        return status_e::failed;
      }

      audio_event.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
      if (!audio_event ||
          FAILED(audio_client->SetEventHandle(audio_event.get())) ||
          FAILED(audio_client->GetBufferSize(&buffer_frames)) ||
          FAILED(audio_client->GetService(
            IID_IAudioRenderClient,
            reinterpret_cast<void **>(&render_client)
          ))) {
        BOOST_LOG(error) << "Deck microphone: unable to create render client";
        return status_e::failed;
      }

      status = audio_client->Start();
      if (FAILED(status)) {
        BOOST_LOG(error) << "Deck microphone: unable to start WASAPI";
        return status_e::failed;
      }

      BOOST_LOG(info) << "Deck microphone: native WASAPI sink active at "
                      << stream.sample_rate << " Hz ("
                      << (stream.codec == codec_e::opus ? "Opus" : "PCM") << ')';
      return status_e::active;
    }

    bool render_until_invalidated(
      stream_t &stream,
      std::stop_token stop_token,
      audio_client_t &audio_client,
      render_client_t &render_client,
      event_t &audio_event,
      UINT32 buffer_frames
    ) {
      while (!stop_token.stop_requested()) {
        const DWORD wait_status = WaitForSingleObject(audio_event.get(), render_wait_ms);
        if (wait_status == WAIT_TIMEOUT) {
          continue;
        }
        if (wait_status != WAIT_OBJECT_0) {
          return false;
        }

        UINT32 padding = 0;
        if (FAILED(audio_client->GetCurrentPadding(&padding))) {
          return false;
        }

        const UINT32 available = buffer_frames > padding ? buffer_frames - padding : 0;
        if (available == 0) {
          continue;
        }

        BYTE *destination = nullptr;
        if (FAILED(render_client->GetBuffer(available, &destination))) {
          return false;
        }

        auto *output = reinterpret_cast<std::int16_t *>(destination);
        UINT32 copied = 0;
        {
          std::lock_guard lock(stream.queue_mutex);
          while (copied < available && !stream.samples.empty()) {
            output[copied++] = stream.samples.front();
            stream.samples.pop_front();
          }
        }

        if (copied == 0) {
          if (FAILED(render_client->ReleaseBuffer(
                available,
                AUDCLNT_BUFFERFLAGS_SILENT
              ))) {
            return false;
          }
        }
        else {
          std::fill(output + copied, output + available, 0);
          if (FAILED(render_client->ReleaseBuffer(available, 0))) {
            return false;
          }
        }
      }

      return true;
    }

    void worker(std::stop_token stop_token, stream_t *stream) {
      const HRESULT com_status = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY
      );
      if (FAILED(com_status)) {
        stream->ready.set_value(status_e::failed);
        return;
      }

      DWORD task_index = 0;
      HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

      device_enum_t enumerator;
      HRESULT status = CoCreateInstance(
        CLSID_MMDeviceEnumerator,
        nullptr,
        CLSCTX_ALL,
        IID_IMMDeviceEnumerator,
        reinterpret_cast<void **>(&enumerator)
      );
      if (FAILED(status)) {
        if (mmcss != nullptr) {
          AvRevertMmThreadCharacteristics(mmcss);
        }
        stream->ready.set_value(status_e::failed);
        CoUninitialize();
        return;
      }

      bool ready_reported = false;
      while (!stop_token.stop_requested()) {
        audio_client_t audio_client;
        render_client_t render_client;
        event_t audio_event;
        UINT32 buffer_frames = 0;
        const status_e endpoint_status = initialize_endpoint(
          *stream,
          enumerator,
          audio_client,
          render_client,
          audio_event,
          buffer_frames
        );

        if (!ready_reported) {
          stream->ready.set_value(endpoint_status);
          ready_reported = true;
          if (endpoint_status != status_e::active) {
            break;
          }
        }
        else if (endpoint_status != status_e::active) {
          std::this_thread::sleep_for(500ms);
          continue;
        }

        const bool clean_stop = render_until_invalidated(
          *stream,
          stop_token,
          audio_client,
          render_client,
          audio_event,
          buffer_frames
        );
        audio_client->Stop();
        if (clean_stop || stop_token.stop_requested()) {
          break;
        }

        BOOST_LOG(warning) << "Deck microphone: endpoint changed, reopening";
        std::this_thread::sleep_for(250ms);
      }

      if (!ready_reported) {
        stream->ready.set_value(status_e::failed);
      }
      if (mmcss != nullptr) {
        AvRevertMmThreadCharacteristics(mmcss);
      }
      CoUninitialize();
    }

    void stop_stream(const std::shared_ptr<stream_t> &stream) {
      if (!stream) {
        return;
      }
      stream->thread.request_stop();
      if (stream->thread.joinable()) {
        stream->thread.join();
      }
    }

    bool supported_format(codec_e codec, std::uint32_t sample_rate) {
      return (codec == codec_e::pcm_s16 && sample_rate == legacy_sample_rate) ||
             (codec == codec_e::opus && sample_rate == opus_sample_rate);
    }

    void append_samples(
      stream_t &stream,
      std::span<const std::int16_t> samples
    ) {
      const std::size_t max_queued_samples = stream.sample_rate / 10;
      if (samples.size() >= max_queued_samples) {
        stream.samples.clear();
        stream.samples.insert(
          stream.samples.end(),
          samples.end() - max_queued_samples,
          samples.end()
        );
        return;
      }

      const std::size_t overflow =
        stream.samples.size() + samples.size() > max_queued_samples ?
          stream.samples.size() + samples.size() - max_queued_samples :
          0;
      for (std::size_t index = 0; index < overflow; ++index) {
        stream.samples.pop_front();
      }
      stream.samples.insert(stream.samples.end(), samples.begin(), samples.end());
    }
  }  // namespace

  status_e start(
    const std::string &owner,
    std::uint64_t generation,
    codec_e codec,
    std::uint32_t sample_rate
  ) {
    if (!supported_format(codec, sample_rate)) {
      return status_e::unsupported;
    }

    std::lock_guard service_lock(service_mutex);
    if (active_stream &&
        active_stream->owner == owner &&
        active_stream->generation == generation &&
        active_stream->codec == codec &&
        active_stream->sample_rate == sample_rate) {
      return status_e::active;
    }

    auto previous = std::move(active_stream);
    stop_stream(previous);

    auto stream = std::make_shared<stream_t>();
    stream->owner = owner;
    stream->generation = generation;
    stream->codec = codec;
    stream->sample_rate = sample_rate;
    if (codec == codec_e::opus) {
      int opus_error = OPUS_OK;
      stream->decoder = opus_decoder_create(
        static_cast<opus_int32>(sample_rate),
        1,
        &opus_error
      );
      if (stream->decoder == nullptr || opus_error != OPUS_OK) {
        BOOST_LOG(error) << "Deck microphone: Opus decoder initialization failed: "
                         << opus_strerror(opus_error);
        return status_e::failed;
      }
    }

    auto ready = stream->ready.get_future();
    stream->thread = std::jthread(worker, stream.get());
    active_stream = stream;

    if (ready.wait_for(2s) != std::future_status::ready) {
      BOOST_LOG(error) << "Deck microphone: endpoint startup timed out";
      active_stream.reset();
      stop_stream(stream);
      return status_e::failed;
    }

    const status_e result = ready.get();
    if (result != status_e::active) {
      active_stream.reset();
      stop_stream(stream);
    }
    return result;
  }

  void submit_pcm(
    const std::string &owner,
    std::uint64_t generation,
    std::uint32_t sequence,
    std::span<const std::int16_t> samples
  ) {
    std::shared_ptr<stream_t> stream;
    {
      std::lock_guard service_lock(service_mutex);
      if (!active_stream ||
          active_stream->owner != owner ||
          active_stream->generation != generation ||
          active_stream->codec != codec_e::pcm_s16) {
        return;
      }
      stream = active_stream;
    }

    {
      std::lock_guard queue_lock(stream->queue_mutex);
      if (stream->have_sequence &&
          static_cast<std::int32_t>(sequence - stream->last_sequence) <= 0) {
        return;
      }
      stream->last_sequence = sequence;
      stream->have_sequence = true;
      append_samples(*stream, samples);
    }
  }

  void submit_opus(
    const std::string &owner,
    std::uint64_t generation,
    std::uint32_t sequence,
    std::span<const std::uint8_t> packet
  ) {
    if (packet.empty() ||
        opus_packet_get_nb_samples(
          packet.data(),
          static_cast<opus_int32>(packet.size()),
          opus_sample_rate
        ) != opus_frame_samples) {
      return;
    }

    std::shared_ptr<stream_t> stream;
    {
      std::lock_guard service_lock(service_mutex);
      if (!active_stream ||
          active_stream->owner != owner ||
          active_stream->generation != generation ||
          active_stream->codec != codec_e::opus ||
          active_stream->decoder == nullptr) {
        return;
      }
      stream = active_stream;
    }

    std::array<std::int16_t, opus_frame_samples> decoded {};
    std::lock_guard queue_lock(stream->queue_mutex);
    std::uint32_t missing_packets = 0;
    if (stream->have_sequence) {
      const auto delta = static_cast<std::int32_t>(sequence - stream->last_sequence);
      if (delta <= 0) {
        return;
      }
      missing_packets = static_cast<std::uint32_t>(delta - 1);
    }

    if (missing_packets > max_concealed_packets) {
      stream->samples.clear();
      opus_decoder_ctl(stream->decoder, OPUS_RESET_STATE);
    }
    else {
      for (std::uint32_t index = 0; index < missing_packets; ++index) {
        const int decoded_samples = opus_decode(
          stream->decoder,
          nullptr,
          0,
          decoded.data(),
          opus_frame_samples,
          0
        );
        if (decoded_samples > 0) {
          append_samples(
            *stream,
            std::span<const std::int16_t> {
              decoded.data(),
              static_cast<std::size_t>(decoded_samples)
            }
          );
        }
      }
    }

    int decoded_samples = opus_decode(
      stream->decoder,
      packet.data(),
      static_cast<opus_int32>(packet.size()),
      decoded.data(),
      opus_frame_samples,
      0
    );
    if (decoded_samples < 0) {
      BOOST_LOG(warning) << "Deck microphone: Opus decode failed: "
                         << opus_strerror(decoded_samples);
      decoded_samples = opus_decode(
        stream->decoder,
        nullptr,
        0,
        decoded.data(),
        opus_frame_samples,
        0
      );
    }

    if (decoded_samples > 0) {
      append_samples(
        *stream,
        std::span<const std::int16_t> {
          decoded.data(),
          static_cast<std::size_t>(decoded_samples)
        }
      );
    }
    stream->last_sequence = sequence;
    stream->have_sequence = true;
  }

  void stop(const std::string &owner, std::uint64_t generation) {
    std::shared_ptr<stream_t> stream;
    {
      std::lock_guard service_lock(service_mutex);
      if (!active_stream ||
          active_stream->owner != owner ||
          active_stream->generation != generation) {
        return;
      }
      stream = std::move(active_stream);
    }
    stop_stream(stream);
  }
}  // namespace platf::deck_microphone
