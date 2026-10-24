#include "pc/controllerspeaker.h"
#include "pc/controllerspeakermixer.h"
#include "pc/settings.h"
#include "pc/audio.h"
#include "pc/log.h"
#include "p3d/context.h"
#include "p3d/input.h"
#include "pddi/gamepadaudio.h"

#if defined(RC_PLATFORM_WINDOWS) && !defined(RC_PLATFORM_NULL)
#define NOMINMAX
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <devpkey.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <wrl/client.h>
#include "miniaudio.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace {
using Microsoft::WRL::ComPtr;
std::mutex mutex;
std::condition_variable wake;
std::thread worker;
bool running = false, deviceReady = false, routed = false;
std::string requestedPath, openedPath;
unsigned int requestedInstance = 0;
unsigned int openedInstance = 0;
float gain = 0;
u64 acceptedEffects = 0;
ControllerSpeakerMixer mixer;
std::atomic<bool> deviceFailed{false};
std::atomic<bool> supportedController{false};

std::wstring Wide(const char* value) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring out(count, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, out.data(), count);
    out.pop_back();
    return out;
}

bool UsbContainer(const std::string& path, GUID& container) {
    const auto wide = Wide(path.c_str());
    HDEVINFO set = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (set == INVALID_HANDLE_VALUE) return false;
    SP_DEVICE_INTERFACE_DATA iface{sizeof(iface)};
    SP_DEVINFO_DATA info{sizeof(info)};
    DWORD required = 0;
    bool found = false;
    if (SetupDiOpenDeviceInterfaceW(set, wide.c_str(), 0, &iface)) {
        SetupDiGetDeviceInterfaceDetailW(set, &iface, nullptr, 0, &required, nullptr);
        if (required && required < 65536) {
            std::vector<unsigned char> bytes(required);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(bytes.data());
            detail->cbSize = sizeof(*detail);
            if (SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, required, nullptr, &info)) {
                DEVINST node = info.DevInst;
                for (int depth = 0; depth < 8; ++depth) {
                    wchar_t id[MAX_DEVICE_ID_LEN]{};
                    if (CM_Get_Device_IDW(node, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) break;
                    std::wstring name(id);
                    for (auto& c : name) c = (wchar_t)towupper(c);
                    if (name.starts_with(L"USB\\VID_054C&PID_0CE6\\")
                        || name.starts_with(L"USB\\VID_054C&PID_0DF2\\")) {
                        DEVPROPTYPE type = 0;
                        ULONG size = sizeof(container);
                        found = CM_Get_DevNode_PropertyW(node, &DEVPKEY_Device_ContainerId, &type,
                            (PBYTE)&container, &size, 0) == CR_SUCCESS && type == DEVPROP_TYPE_GUID
                            && !IsEqualGUID(container, GUID_NULL);
                        break;
                    }
                    DEVINST parent;
                    if (CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) break;
                    node = parent;
                }
            }
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

std::wstring DeviceId(IMMDevice* device) {
    LPWSTR value = nullptr;
    if (!device || FAILED(device->GetId(&value))) return {};
    std::wstring out(value);
    CoTaskMemFree(value);
    return out;
}

bool FindEndpoint(IMMDeviceEnumerator* enumerator, const std::string& path, std::wstring& endpoint) {
    GUID container{};
    if (!UsbContainer(path, container)) return false;
    ComPtr<IMMDevice> primary;
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &primary);
    const auto primaryId = DeviceId(primary.Get());
    if (primaryId.empty()) return false;
    ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) return false;
    UINT count = 0;
    collection->GetCount(&count);
    std::vector<ControllerSpeakerEndpoint> candidates;
    std::vector<std::wstring> ids;
    for (UINT i = 0; i < count; ++i) {
        ControllerSpeakerEndpoint candidate;
        ComPtr<IMMDevice> device;
        ComPtr<IPropertyStore> properties;
        collection->Item(i, &device);
        if (!device || FAILED(device->OpenPropertyStore(STGM_READ, &properties))) continue;
        PROPVARIANT value{};
        properties->GetValue(PKEY_Device_ContainerId, &value);
        candidate.sameContainer = value.vt == VT_CLSID && value.puuid && IsEqualGUID(*value.puuid, container);
        PropVariantClear(&value);
        if (!candidate.sameContainer) continue;
        properties->GetValue(PKEY_AudioEndpoint_FormFactor, &value);
        candidate.speakers = value.vt == VT_UI4 && value.ulVal == Speakers;
        PropVariantClear(&value);
        ComPtr<IAudioClient> client;
        WAVEFORMATEX* format = nullptr;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client))
            && SUCCEEDED(client->GetMixFormat(&format))) {
            candidate.channels = format->nChannels;
            if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE || format->cbSize < 22
                || reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->dwChannelMask != 0x33)
                candidate.channels = 0;
            CoTaskMemFree(format);
        }
        auto id = DeviceId(device.Get());
        if (id.empty()) continue;
        ids.push_back(std::move(id));
        candidate.primary = ids.back() == primaryId;
        candidate.active = true;
        candidates.push_back(candidate);
    }
    const int selected = SelectControllerSpeakerEndpoint(candidates);
    if (selected < 0) return false;
    endpoint = ids[selected];
    ComPtr<IMMDevice> device;
    ComPtr<IAudioEndpointVolume> volume;
    if (SUCCEEDED(enumerator->GetDevice(endpoint.c_str(), &device))
        && SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &volume))) {
        float level = 0;
        BOOL muted = FALSE;
        if (SUCCEEDED(volume->GetMasterVolumeLevelScalar(&level)) && SUCCEEDED(volume->GetMute(&muted)))
            LOG_ALWAYS("[ControllerSpeaker] Windows endpoint volume %.0f%%, muted=%d (read-only)", level*100, (int)muted);
    }
    return true;
}

void Callback(ma_device* device, void* output, const void*, ma_uint32 frames) {
    std::lock_guard lock(mutex);
    mixer.Render((float*)output, frames, device->sampleRate, routed ? gain : 0);
}

void Notify(const ma_device_notification* event) {
    if (event->type == ma_device_notification_type_stopped || event->type == ma_device_notification_type_rerouted
        || event->type == ma_device_notification_type_interruption_began) {
        deviceFailed.store(true);
        wake.notify_one();
    }
}

void Run() {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (SUCCEEDED(com)) CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    ma_device device{};
    ma_context context{};
    const ma_backend backend = ma_backend_wasapi;
    const bool contextReady = ma_context_init(&backend, 1, nullptr, &context) == MA_SUCCESS;
    bool open = false;
    std::wstring endpoint;
    std::string attemptedPath;
    unsigned int attemptedInstance = 0;
    auto retryAt = std::chrono::steady_clock::time_point{};
    for (;;) {
        std::unique_lock lock(mutex);
        if (!running) break;
        const std::string path = requestedPath;
        const unsigned int instance = requestedInstance;
        bool valid = open && !deviceFailed.load() && path == openedPath && instance == openedInstance;
        lock.unlock();
        if (valid && enumerator) {
            ComPtr<IMMDevice> current, primary;
            DWORD state = 0;
            valid = SUCCEEDED(enumerator->GetDevice(endpoint.c_str(), &current))
                && SUCCEEDED(current->GetState(&state)) && state == DEVICE_STATE_ACTIVE;
            enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &primary);
            const auto primaryId = DeviceId(primary.Get());
            valid &= !primaryId.empty() && primaryId != endpoint;
        }
        if (!valid) {
            {
                std::lock_guard guard(mutex);
                deviceReady = routed = false;
                openedPath.clear();
                mixer.Clear();
            }
            if (open) { ma_device_uninit(&device); open = false; }
            deviceFailed.store(false);
            const auto now = std::chrono::steady_clock::now();
            const bool retry = now >= retryAt || attemptedPath != path || attemptedInstance != instance;
            if (retry) {
                attemptedPath = path; attemptedInstance = instance;
                retryAt = now + std::chrono::seconds(2);
            }
            if (retry && !path.empty() && contextReady && enumerator && FindEndpoint(enumerator.Get(), path, endpoint)) {
                ma_device_id id{};
                if (endpoint.size() < std::size(id.wasapi)) {
                    std::copy(endpoint.begin(), endpoint.end(), id.wasapi);
                    auto config = ma_device_config_init(ma_device_type_playback);
                    config.playback.pDeviceID = &id;
                    config.playback.format = ma_format_f32;
                    config.playback.channels = 4;
                    ma_channel channels[]{MA_CHANNEL_FRONT_LEFT, MA_CHANNEL_FRONT_RIGHT,
                        MA_CHANNEL_BACK_LEFT, MA_CHANNEL_BACK_RIGHT};
                    config.playback.pChannelMap = channels;
                    config.sampleRate = 48000;
                    config.periodSizeInFrames = 256;
                    config.periods = 3;
                    config.wasapi.noAutoStreamRouting = MA_TRUE;
                    config.wasapi.noAutoConvertSRC = MA_TRUE;
                    config.dataCallback = Callback;
                    config.notificationCallback = Notify;
                    deviceFailed.store(false);
                    if (ma_device_init(&context, &config, &device) == MA_SUCCESS) {
                        if (device.pContext->backend == ma_backend_wasapi && device.playback.internalChannels == 4
                            && ma_device_start(&device) == MA_SUCCESS) {
                            open = true;
                            std::lock_guard guard(mutex);
                            if (requestedPath == path && requestedInstance == instance) {
                                openedPath = path;
                                openedInstance = instance;
                                deviceReady = true;
                                LOG_ALWAYS("[ControllerSpeaker] Opened matched USB speaker endpoint (4 channels, %u Hz)", device.sampleRate);
                            }
                        }
                        else ma_device_uninit(&device);
                    }
                }
            }
        }
        lock.lock();
        wake.wait_for(lock, std::chrono::seconds(1), [&] {
            return !running || requestedPath != path || requestedInstance != instance || deviceFailed.load();
        });
    }
    {
        std::lock_guard guard(mutex);
        deviceReady = routed = false;
        mixer.Clear();
    }
    if (open) ma_device_uninit(&device);
    if (contextReady) ma_context_uninit(&context);
    enumerator.Reset();
    if (SUCCEEDED(com)) CoUninitialize();
}
}

void ControllerSpeaker::Update() {
    GamepadAudioDevice device;
    const bool identified = p3d::input
        && p3d::input->GetGamepadAudioDevice(device) && device.vendor == 0x054c
        && (device.product == 0x0ce6 || device.product == 0x0df2);
    static std::string capabilityPath;
    static unsigned int capabilityInstance = 0;
    static bool usbCapable = false;
    static auto capabilityRetryAt = std::chrono::steady_clock::time_point{};
    const std::string identityPath = identified ? device.path : "";
    if (identityPath != capabilityPath || device.instance != capabilityInstance
        || (identified && !usbCapable && std::chrono::steady_clock::now() >= capabilityRetryAt)) {
        GUID container{};
        usbCapable = identified && UsbContainer(identityPath, container);
        capabilityPath = identityPath; capabilityInstance = device.instance;
        capabilityRetryAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    }
    supportedController.store(identified && usbCapable);
    const bool supported = g_settings.controllerSpeakerEnabled && supportedController.load();
    const std::string path = supported ? device.path : "";
    static unsigned int routeInstance = 0;
    static std::string routePath;
    static bool routeOwned = false;
    static auto nextRouteAttempt = std::chrono::steady_clock::time_point{};
    std::unique_lock lock(mutex);
    if (path != requestedPath || (supported && device.instance != requestedInstance)) {
        const bool restore = routeOwned && path.empty() && p3d::input
            && p3d::input->GetGamepadAudioDevice(device) && device.instance == routeInstance;
        routed = false;
        mixer.Clear();
        if (restore) p3d::input->SetGamepadSpeakerEnabled(false);
        routeOwned = false;
        requestedPath = path;
        requestedInstance = device.instance;
        nextRouteAttempt = {};
        wake.notify_one();
    }
    // Compensate for the main mixer's nominal 0.5 headroom on this independent mono output.
    gain = g_settings.controllerSpeakerVolume / 100.0f * AudioEngine::GetMasterVolume() * 2.0f;
    if (!path.empty() && !worker.joinable()) {
        running = true;
        worker = std::thread(Run);
    }
    if (!deviceReady || openedPath != path || openedInstance != device.instance || deviceFailed.load()) {
        if (routeOwned && path == routePath && device.instance == routeInstance && p3d::input)
            p3d::input->SetGamepadSpeakerEnabled(false);
        routeOwned = routed = false;
        return;
    }
    if (!routed && !path.empty() && std::chrono::steady_clock::now() >= nextRouteAttempt) {
        routed = p3d::input->SetGamepadSpeakerEnabled(true);
        routeInstance = device.instance;
        routePath = path;
        routeOwned = routed;
        nextRouteAttempt = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }
}

void ControllerSpeaker::Shutdown() {
    {
        std::lock_guard lock(mutex);
        if (routed && p3d::input) p3d::input->SetGamepadSpeakerEnabled(false);
        running = deviceReady = routed = false;
        supportedController.store(false);
        requestedPath.clear(); openedPath.clear();
        mixer.Clear();
    }
    wake.notify_one();
    if (worker.joinable()) worker.join();
}

void ControllerSpeaker::StopAll() { std::lock_guard lock(mutex); mixer.Clear(); }
bool ControllerSpeaker::IsReady() { std::lock_guard lock(mutex); return routed && !deviceFailed.load(); }
bool ControllerSpeaker::IsSupportedController() { return supportedController.load(); }
u64 ControllerSpeaker::AcceptedEffects() { std::lock_guard lock(mutex); return acceptedEffects; }
bool ControllerSpeaker::Play(const s16* pcm, u32 frames, u32 rate, u32 channels, float volume, float pitch) {
    std::lock_guard lock(mutex);
    if (!routed || deviceFailed.load() || !mixer.Play(pcm, frames, rate, channels, volume, pitch)) return false;
    ++acceptedEffects;
    return true;
}
#else
void ControllerSpeaker::Update() {}
void ControllerSpeaker::Shutdown() {}
void ControllerSpeaker::StopAll() {}
bool ControllerSpeaker::IsReady() { return false; }
bool ControllerSpeaker::IsSupportedController() { return false; }
u64 ControllerSpeaker::AcceptedEffects() { return 0; }
bool ControllerSpeaker::Play(const s16*, u32, u32, u32, float, float) { return false; }
#endif
