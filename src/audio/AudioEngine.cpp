// src/audio/AudioEngine.cpp
// Real-time audio engine using miniaudio (WASAPI only).
// Dedicated capture-first routing architecture:
// Captures from microphone -> mono downmix -> DualLayerMixer ->
//   -> cable ring buffer   -> cable playback callback   -> CABLE Input
//   -> monitor ring buffer -> monitor playback callback -> local headphones (optional)

#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <miniaudio.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "lawliet/audio/AudioEngine.hpp"
#include "lawliet/dsp/DualLayerMixer.hpp"
#include "lawliet/dsp/SoundTouchPitchShifter.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lawliet::audio {

// ── Error String ─────────────────────────────────────────────────────────────

std::string toString(AudioEngineError e)
{
    switch (e) {
        case AudioEngineError::None:              return "None";
        case AudioEngineError::DeviceOpenFailed:  return "DeviceOpenFailed";
        case AudioEngineError::DeviceStartFailed: return "DeviceStartFailed";
        case AudioEngineError::InvalidConfig:     return "InvalidConfig";
    }
    return "Unknown";
}

// ── String Helper Functions ──────────────────────────────────────────────────

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool isVirtualCableEndpoint(const std::string& name)
{
    const std::string lower = toLower(name);
    return lower.find("cable") != std::string::npos || lower.find("vb-audio") != std::string::npos;
}

static bool isCableInputEndpoint(const std::string& name)
{
    const std::string lower = toLower(name);
    return lower.find("cable input") != std::string::npos;
}

// ── AudioEngine::Impl ────────────────────────────────────────────────────────

struct AudioEngine::Impl {
    ma_context  context{};
    bool        contextInit = false;

    // Three discrete devices:
    ma_device   captureDevice{};     // Microphone capture
    ma_device   cableDevice{};       // Virtual Cable output (streams into Discord / apps)
    ma_device   monitorDevice{};     // Optional local headphone playback

    bool        captureInit = false;
    bool        cableInit   = false;
    bool        monitorInit = false;

    // Engine status & state (audio thread never touches mutex or strings)
    std::atomic<State> state{State::Stopped};
    mutable std::mutex  statusMutex;
    std::string         statusMessage{"Stopped"};
    std::string         lastError;

    std::unique_ptr<dsp::DualLayerMixer> mixer;

    config::Settings currentSettings;

    std::string activeCaptureName;
    std::string activeCableName;
    std::string activeMonitorName;

    std::atomic<bool>  monitorEnabled{false};
    std::atomic<float> monitorVolume{0.8f};

    // Live RMS meters (0.0 to 1.0)
    std::atomic<float> inputRMS{0.0f};
    std::atomic<float> outputRMS{0.0f};

    // Scratch buffers for channel conversion and processing
    static constexpr std::size_t kScratchFrames = dsp::DualLayerMixer::kMaxFrames;
    float captureMono[kScratchFrames]  = {};
    float playbackMono[kScratchFrames] = {};

    // Dedicated ring buffer for VB-Cable output
    static constexpr std::size_t kCableRingFrames = 32768;
    float cableRing[kCableRingFrames] = {};
    std::atomic<std::size_t> cableWriteIdx{0};
    std::atomic<std::size_t> cableReadIdx{0};
    std::atomic<bool>        cablePrebuffered{false};

    // Dedicated ring buffer for local headphone monitor
    static constexpr std::size_t kMonitorRingFrames = 32768;
    float monitorRing[kMonitorRingFrames] = {};
    std::atomic<std::size_t> monitorWriteIdx{0};
    std::atomic<std::size_t> monitorReadIdx{0};

    void setStatus(State s, const std::string& msg) {
        state.store(s, std::memory_order_release);
        std::lock_guard<std::mutex> lock(statusMutex);
        statusMessage = msg;
        if (s == State::Error) {
            lastError = msg;
        } else {
            lastError.clear();
        }
    }

    bool initContext();
    bool initCapture(const std::string& targetMic);
    bool initCable(const std::string& targetCable);
    bool initMonitor(const std::string& targetMonitor);
};

// ── Audio Callbacks (Strict Real-Time Hygiene) ─────────────────────────────────
// No new/malloc, no std::string, no vector growth, no mutexes, no printf, no I/O.

static void captureCallback(ma_device* pDevice, void* /*pOutput*/, const void* pInput, ma_uint32 frameCount)
{
    auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
    if (!impl || !impl->mixer || !pInput) return;

    const auto* inF = static_cast<const float*>(pInput);
    const std::size_t frames = static_cast<std::size_t>(frameCount);
    if (frames > AudioEngine::Impl::kScratchFrames) return;

    const std::uint32_t capChannels = pDevice->capture.channels;

    // 1. Downmix capture to mono scratch buffer
    if (capChannels == 1) {
        std::memcpy(impl->captureMono, inF, frames * sizeof(float));
    } else {
        const float inv = 1.0f / static_cast<float>(capChannels);
        for (std::size_t f = 0; f < frames; ++f) {
            float sum = 0.0f;
            for (std::uint32_t ch = 0; ch < capChannels; ++ch) {
                sum += inF[f * capChannels + ch];
            }
            impl->captureMono[f] = sum * inv;
        }
    }

    // 2. Input RMS for live level meter
    float inSumSq = 0.0f;
    for (std::size_t f = 0; f < frames; ++f) {
        const float inS = impl->captureMono[f];
        inSumSq += inS * inS;
    }
    impl->inputRMS.store(frames ? std::sqrt(inSumSq / static_cast<float>(frames)) : 0.0f,
                         std::memory_order_relaxed);

    // 3. Process DSP pipeline (DualLayerMixer with SoundTouch pitch shifters)
    impl->mixer->process(impl->captureMono, impl->playbackMono, frames);

    // 4. Output RMS for live level meter
    float outSumSq = 0.0f;
    for (std::size_t f = 0; f < frames; ++f) {
        const float outS = impl->playbackMono[f];
        outSumSq += outS * outS;
    }
    impl->outputRMS.store(frames ? std::sqrt(outSumSq / static_cast<float>(frames)) : 0.0f,
                          std::memory_order_relaxed);

    // 5. Push processed audio to VB-Cable ring buffer
    if (impl->cableInit) {
        std::size_t w = impl->cableWriteIdx.load(std::memory_order_relaxed);
        for (std::size_t f = 0; f < frames; ++f) {
            impl->cableRing[w] = impl->playbackMono[f];
            w = (w + 1) % AudioEngine::Impl::kCableRingFrames;
        }
        impl->cableWriteIdx.store(w, std::memory_order_release);
    }

    // 6. Push to headphone monitor ring buffer if enabled
    if (impl->monitorEnabled.load(std::memory_order_relaxed)) {
        std::size_t w = impl->monitorWriteIdx.load(std::memory_order_relaxed);
        for (std::size_t f = 0; f < frames; ++f) {
            impl->monitorRing[w] = impl->playbackMono[f];
            w = (w + 1) % AudioEngine::Impl::kMonitorRingFrames;
        }
        impl->monitorWriteIdx.store(w, std::memory_order_release);
    }
}

static void cableCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount)
{
    auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
    auto* outF = static_cast<float*>(pOutput);
    const std::uint32_t pbChannels = pDevice->playback.channels;
    const std::size_t frames = static_cast<std::size_t>(frameCount);

    if (!impl || !outF) return;

    if (impl->state.load(std::memory_order_relaxed) != State::Running) {
        std::memset(outF, 0, frames * pbChannels * sizeof(float));
        return;
    }

    std::size_t r = impl->cableReadIdx.load(std::memory_order_relaxed);
    const std::size_t w = impl->cableWriteIdx.load(std::memory_order_acquire);
    const std::size_t available = (w >= r) ? (w - r) : (AudioEngine::Impl::kCableRingFrames - r + w);

    // Require ~1024 frames (~23ms cushion) before streaming to prevent underflow jitter
    static constexpr std::size_t kPrebufferFrames = 1024;
    if (!impl->cablePrebuffered.load(std::memory_order_relaxed)) {
        if (available >= kPrebufferFrames) {
            impl->cablePrebuffered.store(true, std::memory_order_release);
        } else {
            std::memset(outF, 0, frames * pbChannels * sizeof(float));
            return;
        }
    }

    for (std::size_t f = 0; f < frames; ++f) {
        float sample = 0.0f;
        if (r != w) {
            sample = impl->cableRing[r];
            r = (r + 1) % AudioEngine::Impl::kCableRingFrames;
        }
        for (std::uint32_t ch = 0; ch < pbChannels; ++ch) {
            outF[f * pbChannels + ch] = sample;
        }
    }
    impl->cableReadIdx.store(r, std::memory_order_release);

    if (r == w) {
        // Underflow: wait for jitter cushion to refill before unmuting
        impl->cablePrebuffered.store(false, std::memory_order_release);
    }
}

static void monitorCallback(ma_device* pDevice, void* pOutput, const void* /*pInput*/, ma_uint32 frameCount)
{
    auto* impl = static_cast<AudioEngine::Impl*>(pDevice->pUserData);
    auto* outF = static_cast<float*>(pOutput);
    const std::uint32_t pbChannels = pDevice->playback.channels;
    const std::size_t frames = static_cast<std::size_t>(frameCount);

    if (!impl || !outF || !impl->monitorEnabled.load(std::memory_order_relaxed) ||
        impl->state.load(std::memory_order_relaxed) != State::Running) {
        if (outF) std::memset(outF, 0, frames * pbChannels * sizeof(float));
        return;
    }

    const float vol = impl->monitorVolume.load(std::memory_order_relaxed);
    std::size_t r = impl->monitorReadIdx.load(std::memory_order_relaxed);
    const std::size_t w = impl->monitorWriteIdx.load(std::memory_order_acquire);

    for (std::size_t f = 0; f < frames; ++f) {
        float sample = 0.0f;
        if (r != w) {
            sample = impl->monitorRing[r] * vol;
            r = (r + 1) % AudioEngine::Impl::kMonitorRingFrames;
        }
        for (std::uint32_t ch = 0; ch < pbChannels; ++ch) {
            outF[f * pbChannels + ch] = sample;
        }
    }
    impl->monitorReadIdx.store(r, std::memory_order_release);
}

// ── Device Initialization Helpers ─────────────────────────────────────────────

bool AudioEngine::Impl::initContext()
{
    if (contextInit) return true;

    // Step 2: WASAPI only backend
    ma_backend backends[] = { ma_backend_wasapi };
    if (ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), nullptr, &context) != MA_SUCCESS) {
        setStatus(State::Error, "Failed to initialize WASAPI audio context");
        return false;
    }
    contextInit = true;
    return true;
}

bool AudioEngine::Impl::initCapture(const std::string& targetMic)
{
    if (!contextInit && !initContext()) return false;
    if (captureInit) {
        ma_device_uninit(&captureDevice);
        captureInit = false;
    }

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        setStatus(State::Error, "Failed to query audio devices");
        return false;
    }

    const ma_device_id* pChosenId = nullptr;
    std::string chosenName;

    // Step 5: Saved device by name if it still exists (and is not a CABLE endpoint)
    if (!targetMic.empty() && targetMic != "Default" && targetMic != "(Default)" &&
        targetMic != "(Default System Microphone)") {
        for (ma_uint32 i = 0; i < capCount; ++i) {
            const std::string dName = pCapture[i].name;
            if (isVirtualCableEndpoint(dName)) continue;
            if (dName == targetMic || dName.find(targetMic) != std::string::npos || targetMic.find(dName) != std::string::npos) {
                pChosenId = &pCapture[i].id;
                chosenName = dName;
                break;
            }
        }
    }

    // Step 5: Else system default capture device (never a CABLE endpoint)
    if (!pChosenId) {
        for (ma_uint32 i = 0; i < capCount; ++i) {
            if (pCapture[i].isDefault) {
                const std::string dName = pCapture[i].name;
                if (!isVirtualCableEndpoint(dName)) {
                    pChosenId = &pCapture[i].id;
                    chosenName = dName;
                }
                break;
            }
        }
    }

    // Step 5: Fallback to first non-CABLE capture device
    if (!pChosenId) {
        for (ma_uint32 i = 0; i < capCount; ++i) {
            const std::string dName = pCapture[i].name;
            if (!isVirtualCableEndpoint(dName)) {
                pChosenId = &pCapture[i].id;
                chosenName = dName;
                break;
            }
        }
    }

    if (!pChosenId) {
        setStatus(State::Error, "No microphone found (virtual cable endpoints excluded)");
        return false;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID       = pChosenId;
    cfg.capture.format          = ma_format_f32;
    cfg.capture.channels        = 1;
    cfg.capture.shareMode       = ma_share_mode_shared;
    cfg.sampleRate              = 44100;
    cfg.periodSizeInFrames      = 256;
    cfg.dataCallback            = captureCallback;
    cfg.pUserData               = this;

    if (ma_device_init(&context, &cfg, &captureDevice) != MA_SUCCESS) {
        setStatus(State::Error, "Failed to initialize microphone device");
        return false;
    }

    captureInit = true;
    activeCaptureName = chosenName;
    return true;
}

bool AudioEngine::Impl::initCable(const std::string& targetCable)
{
    if (!contextInit && !initContext()) return false;
    if (cableInit) {
        ma_device_uninit(&cableDevice);
        cableInit = false;
    }

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        return false;
    }

    const ma_device_id* pChosenId = nullptr;
    std::string chosenName;

    // Step 5: Saved device by name if it exists
    if (!targetCable.empty() && targetCable != "Default" && targetCable != "(Default)") {
        for (ma_uint32 i = 0; i < pbCount; ++i) {
            const std::string dName = pPlayback[i].name;
            if (dName == targetCable || dName.find(targetCable) != std::string::npos || targetCable.find(dName) != std::string::npos) {
                pChosenId = &pPlayback[i].id;
                chosenName = dName;
                break;
            }
        }
    }

    // Step 5: Else a playback device whose name contains "CABLE Input"
    if (!pChosenId) {
        for (ma_uint32 i = 0; i < pbCount; ++i) {
            const std::string dName = pPlayback[i].name;
            if (isCableInputEndpoint(dName)) {
                pChosenId = &pPlayback[i].id;
                chosenName = dName;
                break;
            }
        }
    }

    // Step 5: Else system default playback device
    if (!pChosenId) {
        for (ma_uint32 i = 0; i < pbCount; ++i) {
            if (pPlayback[i].isDefault) {
                pChosenId = &pPlayback[i].id;
                chosenName = pPlayback[i].name;
                break;
            }
        }
        if (!pChosenId && pbCount > 0) {
            pChosenId = &pPlayback[0].id;
            chosenName = pPlayback[0].name;
        }
    }

    if (!pChosenId) {
        return false;
    }

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID      = pChosenId;
    cfg.playback.format         = ma_format_f32;
    cfg.playback.channels       = 2;
    cfg.playback.shareMode      = ma_share_mode_shared;
    cfg.sampleRate              = 44100;
    cfg.periodSizeInFrames      = 256;
    cfg.dataCallback            = cableCallback;
    cfg.pUserData               = this;

    if (ma_device_init(&context, &cfg, &cableDevice) != MA_SUCCESS) {
        return false;
    }

    cableInit = true;
    activeCableName = chosenName;
    return true;
}

bool AudioEngine::Impl::initMonitor(const std::string& targetMonitor)
{
    if (!contextInit && !initContext()) return false;
    if (monitorInit) {
        ma_device_uninit(&monitorDevice);
        monitorInit = false;
    }

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        return false;
    }

    const ma_device_id* pChosenId = nullptr;
    std::string chosenName;

    // 1. Saved monitor device by name if it exists
    if (!targetMonitor.empty() && targetMonitor != "Default" && targetMonitor != "(Default)") {
        for (ma_uint32 i = 0; i < pbCount; ++i) {
            const std::string dName = pPlayback[i].name;
            if (dName == targetMonitor || dName.find(targetMonitor) != std::string::npos || targetMonitor.find(dName) != std::string::npos) {
                pChosenId = &pPlayback[i].id;
                chosenName = dName;
                break;
            }
        }
    }

    // 2. Default playback device
    if (!pChosenId) {
        for (ma_uint32 i = 0; i < pbCount; ++i) {
            if (pPlayback[i].isDefault) {
                pChosenId = &pPlayback[i].id;
                chosenName = pPlayback[i].name;
                break;
            }
        }
        if (!pChosenId && pbCount > 0) {
            pChosenId = &pPlayback[0].id;
            chosenName = pPlayback[0].name;
        }
    }

    if (!pChosenId) return false;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID      = pChosenId;
    cfg.playback.format         = ma_format_f32;
    cfg.playback.channels       = 2;
    cfg.playback.shareMode      = ma_share_mode_shared;
    cfg.sampleRate              = 44100;
    cfg.periodSizeInFrames      = 256;
    cfg.dataCallback            = monitorCallback;
    cfg.pUserData               = this;

    if (ma_device_init(&context, &cfg, &monitorDevice) != MA_SUCCESS) {
        return false;
    }

    monitorInit = true;
    activeMonitorName = chosenName;
    return true;
}

// ── AudioEngine Public Implementation ─────────────────────────────────────────

AudioEngine::AudioEngine()
    : m_impl(new Impl{})
{}

AudioEngine::~AudioEngine()
{
    shutdown();
    delete m_impl;
}

AudioEngineError AudioEngine::init(const config::Settings& settings)
{
    const std::uint32_t sampleRate = 44100;

    // 1. Initialize DSP mixer
    m_impl->mixer = std::make_unique<dsp::DualLayerMixer>(
        std::make_unique<dsp::SoundTouchPitchShifter>(),
        std::make_unique<dsp::SoundTouchPitchShifter>()
    );
    m_impl->mixer->setSampleRate(sampleRate);

    // Apply DSP settings
    m_impl->mixer->setMode(settings.mode == 3 ? dsp::Mode::III : dsp::Mode::II);
    m_impl->mixer->setLowSemitones(settings.lowSemitones);
    m_impl->mixer->setLowGain(settings.lowGain);
    m_impl->mixer->setHighSemitones(settings.highSemitones);
    m_impl->mixer->setHighGain(settings.highGain);
    m_impl->mixer->setDryGain(settings.dryGain);
    m_impl->mixer->setMasterGain(settings.masterGain);

    m_impl->monitorEnabled.store(settings.monitorEnabled, std::memory_order_relaxed);
    m_impl->monitorVolume.store(settings.monitorVolume, std::memory_order_relaxed);
    m_impl->currentSettings = settings;

    // 2. Initialize WASAPI context
    if (!m_impl->initContext()) {
        return AudioEngineError::DeviceOpenFailed;
    }

    // 3. Initialize microphone capture device
    if (!m_impl->initCapture(settings.inputDevice)) {
        return AudioEngineError::DeviceOpenFailed;
    }

    // 4. Initialize cable playback device
    m_impl->initCable(settings.outputDevice);

    // 5. Initialize monitor playback device if enabled
    if (m_impl->monitorEnabled.load()) {
        m_impl->initMonitor(settings.monitorDevice);
    }

    m_impl->setStatus(State::Stopped, "Stopped");
    return AudioEngineError::None;
}

bool AudioEngine::start()
{
    if (!m_impl || !m_impl->captureInit) {
        if (m_impl) m_impl->setStatus(State::Error, "AudioEngine not initialized");
        return false;
    }

    if (m_impl->state.load(std::memory_order_relaxed) == State::Running) {
        return true;
    }

    // Reset ring buffer positions and prebuffer flag before starting
    m_impl->cableWriteIdx.store(0, std::memory_order_release);
    m_impl->cableReadIdx.store(0, std::memory_order_release);
    m_impl->cablePrebuffered.store(false, std::memory_order_release);

    m_impl->monitorWriteIdx.store(0, std::memory_order_release);
    m_impl->monitorReadIdx.store(0, std::memory_order_release);

    // Step 7: 4-attempt retry on capture device start
    ma_result startRes = MA_ERROR;
    for (int attempt = 0; attempt < 4; ++attempt) {
        startRes = ma_device_start(&m_impl->captureDevice);
        if (startRes == MA_SUCCESS) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if (startRes != MA_SUCCESS) {
        m_impl->setStatus(State::Error, "Failed to start capture device");
        return false;
    }

    // Start cable playback device with 4-attempt retry
    if (m_impl->cableInit) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (ma_device_start(&m_impl->cableDevice) == MA_SUCCESS) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    // Start monitor playback device with 4-attempt retry if enabled
    if (m_impl->monitorInit && m_impl->monitorEnabled.load()) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (ma_device_start(&m_impl->monitorDevice) == MA_SUCCESS) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    m_impl->setStatus(State::Running, "Running");
    return true;
}

void AudioEngine::stop()
{
    if (!m_impl || m_impl->state.load(std::memory_order_relaxed) == State::Stopped) {
        return;
    }

    if (m_impl->captureInit) {
        ma_device_stop(&m_impl->captureDevice);
    }
    if (m_impl->cableInit) {
        ma_device_stop(&m_impl->cableDevice);
    }
    if (m_impl->monitorInit) {
        ma_device_stop(&m_impl->monitorDevice);
    }

    m_impl->cablePrebuffered.store(false, std::memory_order_release);
    m_impl->setStatus(State::Stopped, "Stopped");
}

void AudioEngine::shutdown()
{
    if (!m_impl) return;
    stop();

    if (m_impl->captureInit) {
        ma_device_uninit(&m_impl->captureDevice);
        m_impl->captureInit = false;
    }
    if (m_impl->cableInit) {
        ma_device_uninit(&m_impl->cableDevice);
        m_impl->cableInit = false;
    }
    if (m_impl->monitorInit) {
        ma_device_stop(&m_impl->monitorDevice);
        ma_device_uninit(&m_impl->monitorDevice);
        m_impl->monitorInit = false;
    }
    if (m_impl->contextInit) {
        ma_context_uninit(&m_impl->context);
        m_impl->contextInit = false;
    }
}

bool AudioEngine::applySettings(const config::Settings& settings)
{
    if (!m_impl || !m_impl->mixer) return false;

    // Apply DSP settings atomically without device restarts
    m_impl->mixer->setMode(settings.mode == 3 ? dsp::Mode::III : dsp::Mode::II);
    m_impl->mixer->setLowSemitones(settings.lowSemitones);
    m_impl->mixer->setLowGain(settings.lowGain);
    m_impl->mixer->setHighSemitones(settings.highSemitones);
    m_impl->mixer->setHighGain(settings.highGain);
    m_impl->mixer->setDryGain(settings.dryGain);
    m_impl->mixer->setMasterGain(settings.masterGain);

    setMonitorVolume(settings.monitorVolume);
    setMonitorEnabled(settings.monitorEnabled);

    // Apply device changes if requested
    bool devChanged = false;
    std::string newMic = m_impl->currentSettings.inputDevice;
    std::string newOut = m_impl->currentSettings.outputDevice;
    std::string newMon = m_impl->currentSettings.monitorDevice;

    if (!settings.inputDevice.empty() && settings.inputDevice != m_impl->currentSettings.inputDevice) {
        newMic = settings.inputDevice;
        devChanged = true;
    }
    if (!settings.outputDevice.empty() && settings.outputDevice != m_impl->currentSettings.outputDevice) {
        newOut = settings.outputDevice;
        devChanged = true;
    }
    if (!settings.monitorDevice.empty() && settings.monitorDevice != m_impl->currentSettings.monitorDevice) {
        newMon = settings.monitorDevice;
        devChanged = true;
    }

    if (devChanged) {
        return setDevices(newMic, newOut, newMon);
    }

    m_impl->currentSettings = settings;
    return true;
}

config::Settings AudioEngine::exportSettings() const
{
    config::Settings s;
    if (m_impl) {
        s = m_impl->currentSettings;
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) {
            s.mode          = (m->mode() == dsp::Mode::III) ? 3 : 2;
            s.lowSemitones  = m->lowSemitones();
            s.highSemitones = m->highSemitones();
            s.lowGain       = m->lowGain();
            s.highGain      = m->highGain();
            s.dryGain       = m->dryGain();
            s.masterGain    = m->masterGain();
        }
        s.inputDevice    = getActiveCaptureDevice();
        s.outputDevice   = getActiveOutputDevice();
        s.monitorDevice  = m_impl->currentSettings.monitorDevice;
        s.monitorEnabled = isMonitorEnabled();
        s.monitorVolume  = getMonitorVolume();
    }
    return s;
}

dsp::DualLayerMixer* AudioEngine::mixer() noexcept
{
    return m_impl ? m_impl->mixer.get() : nullptr;
}

State AudioEngine::getState() const noexcept
{
    return m_impl ? m_impl->state.load(std::memory_order_acquire) : State::Stopped;
}

std::string AudioEngine::getStatusMessage() const
{
    if (!m_impl) return "Uninitialized";
    std::lock_guard<std::mutex> lock(m_impl->statusMutex);
    return m_impl->statusMessage;
}

bool AudioEngine::isRunning() const noexcept
{
    return getState() == State::Running;
}

const std::string& AudioEngine::getLastError() const noexcept
{
    static const std::string s_none;
    return m_impl ? m_impl->lastError : s_none;
}

float AudioEngine::getInputLevel() const noexcept
{
    return m_impl ? m_impl->inputRMS.load(std::memory_order_relaxed) : 0.0f;
}

float AudioEngine::getOutputLevel() const noexcept
{
    return m_impl ? m_impl->outputRMS.load(std::memory_order_relaxed) : 0.0f;
}

bool AudioEngine::isBypass() const noexcept
{
    return (m_impl && m_impl->mixer) ? m_impl->mixer->isBypass() : false;
}

void AudioEngine::setBypass(bool bypass) noexcept
{
    if (m_impl && m_impl->mixer) m_impl->mixer->setBypass(bypass);
}

bool AudioEngine::isMonitorEnabled() const noexcept
{
    return m_impl ? m_impl->monitorEnabled.load(std::memory_order_relaxed) : false;
}

void AudioEngine::setMonitorEnabled(bool enabled) noexcept
{
    if (!m_impl) return;
    const bool wasEnabled = m_impl->monitorEnabled.exchange(enabled);
    if (wasEnabled == enabled) return;

    if (enabled) {
        if (!m_impl->monitorInit) {
            m_impl->initMonitor(m_impl->currentSettings.monitorDevice);
        }
        if (m_impl->monitorInit && m_impl->state.load() == State::Running) {
            for (int attempt = 0; attempt < 4; ++attempt) {
                if (ma_device_start(&m_impl->monitorDevice) == MA_SUCCESS) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }
    } else {
        if (m_impl->monitorInit) {
            ma_device_stop(&m_impl->monitorDevice);
            ma_device_uninit(&m_impl->monitorDevice);
            m_impl->monitorInit = false;
            m_impl->activeMonitorName = "(Monitor Off)";
        }
    }
}

float AudioEngine::getMonitorVolume() const noexcept
{
    return m_impl ? m_impl->monitorVolume.load(std::memory_order_relaxed) : 0.8f;
}

void AudioEngine::setMonitorVolume(float vol) noexcept
{
    if (m_impl) m_impl->monitorVolume.store(std::clamp(vol, 0.0f, 1.0f), std::memory_order_relaxed);
}

// ── Device Listing & Filter ──────────────────────────────────────────────────

std::vector<AudioDevice> AudioEngine::listCaptureDevices()
{
    std::vector<AudioDevice> result;
    if (!m_impl || (!m_impl->contextInit && !m_impl->initContext())) return result;

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&m_impl->context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        return result;
    }

    for (ma_uint32 i = 0; i < capCount; ++i) {
        const std::string name = pCapture[i].name;
        // Step 4: Hide virtual endpoints (CABLE, VB-Audio) from microphone list
        if (isVirtualCableEndpoint(name)) continue;

        AudioDevice d;
        d.name    = name;
        d.id      = name;
        d.isInput = true;
        result.push_back(std::move(d));
    }

    return result;
}

std::vector<AudioDevice> AudioEngine::listPlaybackDevices()
{
    std::vector<AudioDevice> result;
    if (!m_impl || (!m_impl->contextInit && !m_impl->initContext())) return result;

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&m_impl->context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        return result;
    }

    for (ma_uint32 i = 0; i < pbCount; ++i) {
        AudioDevice d;
        d.name    = pPlayback[i].name;
        d.id      = pPlayback[i].name;
        d.isInput = false;
        result.push_back(std::move(d));
    }

    return result;
}

std::vector<AudioDevice> AudioEngine::enumerateDevices()
{
    return listCaptureDevices();
}

bool AudioEngine::isCablePresent() const noexcept
{
    if (!m_impl || (!m_impl->contextInit && !const_cast<AudioEngine*>(this)->m_impl->initContext())) {
        return false;
    }

    ma_device_info* pCapture  = nullptr;
    ma_uint32       capCount  = 0;
    ma_device_info* pPlayback = nullptr;
    ma_uint32       pbCount   = 0;

    if (ma_context_get_devices(&m_impl->context, &pPlayback, &pbCount, &pCapture, &capCount) != MA_SUCCESS) {
        return false;
    }

    for (ma_uint32 i = 0; i < pbCount; ++i) {
        if (isCableInputEndpoint(pPlayback[i].name)) {
            return true;
        }
    }
    return false;
}

bool AudioEngine::isVirtualCableActive() const noexcept
{
    return m_impl && m_impl->cableInit;
}

const std::string& AudioEngine::getVirtualCableName() const noexcept
{
    static const std::string s_none = "Not Connected";
    return (m_impl && !m_impl->activeCableName.empty()) ? m_impl->activeCableName : s_none;
}

// ── Changing Devices (Step 9) ────────────────────────────────────────────────

bool AudioEngine::setCaptureDevice(const std::string& captureName)
{
    if (!m_impl) return false;
    const bool wasRunning = (m_impl->state.load() == State::Running);
    if (wasRunning) {
        stop();
    }

    if (m_impl->captureInit) {
        ma_device_uninit(&m_impl->captureDevice);
        m_impl->captureInit = false;
    }

    // Step 7/9: 50 ms teardown grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const bool ok = m_impl->initCapture(captureName);
    m_impl->currentSettings.inputDevice = captureName;

    if (wasRunning && ok) {
        start();
    }
    return ok;
}

bool AudioEngine::setOutputDevice(const std::string& outputName)
{
    if (!m_impl) return false;
    const bool wasRunning = (m_impl->state.load() == State::Running);
    if (wasRunning) {
        stop();
    }

    if (m_impl->cableInit) {
        ma_device_uninit(&m_impl->cableDevice);
        m_impl->cableInit = false;
    }

    // Step 7/9: 50 ms teardown grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const bool ok = m_impl->initCable(outputName);
    m_impl->currentSettings.outputDevice = outputName;

    if (wasRunning && ok) {
        start();
    }
    return ok;
}

bool AudioEngine::setMonitorDevice(const std::string& monitorName)
{
    if (!m_impl) return false;
    const bool wasRunning = (m_impl->state.load() == State::Running);

    if (m_impl->monitorInit) {
        ma_device_stop(&m_impl->monitorDevice);
        ma_device_uninit(&m_impl->monitorDevice);
        m_impl->monitorInit = false;
    }

    // Step 7/9: 50 ms teardown grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    m_impl->currentSettings.monitorDevice = monitorName;

    bool ok = true;
    if (m_impl->monitorEnabled.load()) {
        ok = m_impl->initMonitor(monitorName);
        if (wasRunning && ok) {
            for (int attempt = 0; attempt < 4; ++attempt) {
                if (ma_device_start(&m_impl->monitorDevice) == MA_SUCCESS) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        }
    }
    return ok;
}

bool AudioEngine::setDevices(const std::string& captureName, const std::string& outputName, const std::string& monitorName)
{
    if (!m_impl) return false;
    const bool wasRunning = (m_impl->state.load() == State::Running);
    if (wasRunning) {
        stop();
    }

    if (m_impl->captureInit) {
        ma_device_uninit(&m_impl->captureDevice);
        m_impl->captureInit = false;
    }
    if (m_impl->cableInit) {
        ma_device_uninit(&m_impl->cableDevice);
        m_impl->cableInit = false;
    }
    if (m_impl->monitorInit) {
        ma_device_uninit(&m_impl->monitorDevice);
        m_impl->monitorInit = false;
    }

    // Step 7/9: 50 ms teardown grace period
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool ok = true;
    if (!captureName.empty()) {
        ok = m_impl->initCapture(captureName) && ok;
        m_impl->currentSettings.inputDevice = captureName;
    }
    if (!outputName.empty()) {
        ok = m_impl->initCable(outputName) && ok;
        m_impl->currentSettings.outputDevice = outputName;
    }
    if (!monitorName.empty()) {
        m_impl->currentSettings.monitorDevice = monitorName;
        if (m_impl->monitorEnabled.load()) {
            ok = m_impl->initMonitor(monitorName) && ok;
        }
    }

    if (wasRunning && ok) {
        start();
    }
    return ok;
}

// ── Active Device Names ──────────────────────────────────────────────────────

const std::string& AudioEngine::getActiveCaptureDevice() const noexcept
{
    static const std::string s_default = "(Default System Microphone)";
    return (m_impl && !m_impl->activeCaptureName.empty()) ? m_impl->activeCaptureName : s_default;
}

const std::string& AudioEngine::getActiveOutputDevice() const noexcept
{
    static const std::string s_default = "(Default Audio Output)";
    return (m_impl && !m_impl->activeCableName.empty()) ? m_impl->activeCableName : s_default;
}

const std::string& AudioEngine::getActivePlaybackDevice() const noexcept
{
    if (m_impl && m_impl->monitorEnabled.load() && !m_impl->activeMonitorName.empty()) {
        return m_impl->activeMonitorName;
    }
    return getActiveOutputDevice();
}

const std::string& AudioEngine::getActiveMonitorDevice() const noexcept
{
    static const std::string s_off = "(Monitor Off)";
    if (m_impl && m_impl->monitorEnabled.load() && !m_impl->activeMonitorName.empty()) {
        return m_impl->activeMonitorName;
    }
    return s_off;
}

std::uint32_t AudioEngine::getSampleRate() const noexcept
{
    return 44100;
}

std::uint32_t AudioEngine::getFrameSize() const noexcept
{
    return 256;
}

} // namespace lawliet::audio
