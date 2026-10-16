#pragma once

#include "lawliet/config/Settings.hpp"
#include "lawliet/dsp/DualLayerMixer.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lawliet::audio {

enum class AudioEngineError {
    None,
    DeviceOpenFailed,
    DeviceStartFailed,
    InvalidConfig
};

std::string toString(AudioEngineError e);

enum class State {
    Stopped,
    Running,
    Error
};

struct AudioDevice {
    std::string name;
    std::string id;
    bool        isInput = false;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    AudioEngineError init(const config::Settings& settings = config::Settings{});
    bool start();
    void stop();
    void shutdown();

    bool applySettings(const config::Settings& settings);
    config::Settings exportSettings() const;

    dsp::DualLayerMixer* mixer() noexcept;

    // ── Engine State & Status ──────────────────────────────────────────────
    State getState() const noexcept;
    std::string getStatusMessage() const;
    bool isRunning() const noexcept;
    const std::string& getLastError() const noexcept;

    // ── Live RMS Meters (0.0 - 1.0) ────────────────────────────────────────
    float getInputLevel() const noexcept;
    float getOutputLevel() const noexcept;

    // ── Bypass ─────────────────────────────────────────────────────────────
    bool isBypass() const noexcept;
    void setBypass(bool bypass) noexcept;

    // ── Headphone Monitoring ───────────────────────────────────────────────
    bool isMonitorEnabled() const noexcept;
    void setMonitorEnabled(bool enabled) noexcept;
    float getMonitorVolume() const noexcept;
    void setMonitorVolume(float vol) noexcept;

    // ── Device Enumeration & Selection ─────────────────────────────────────
    std::vector<AudioDevice> listCaptureDevices();
    std::vector<AudioDevice> listPlaybackDevices();
    std::vector<AudioDevice> enumerateDevices(); // Alias to listCaptureDevices for backward compatibility

    bool isCablePresent() const noexcept;
    bool isVirtualCableActive() const noexcept;
    const std::string& getVirtualCableName() const noexcept;

    bool setDevices(const std::string& captureName, const std::string& outputName, const std::string& monitorName = "");
    bool setCaptureDevice(const std::string& captureName);
    bool setOutputDevice(const std::string& outputName);
    bool setMonitorDevice(const std::string& monitorName);

    const std::string& getActiveCaptureDevice() const noexcept;
    const std::string& getActiveOutputDevice() const noexcept;
    const std::string& getActivePlaybackDevice() const noexcept;
    const std::string& getActiveMonitorDevice() const noexcept;

    std::uint32_t getSampleRate() const noexcept;
    std::uint32_t getFrameSize() const noexcept;

    // ── Mode II / Mode III ─────────────────────────────────────────────────
    void setMode(dsp::Mode mode) noexcept {
        if (auto* m = mixer()) m->setMode(mode);
    }
    void setMode(int mode) noexcept {
        if (auto* m = mixer()) m->setMode(mode);
    }
    dsp::Mode getMode() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->mode();
        return dsp::Mode::II;
    }
    bool isModeIII() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->isModeIII();
        return false;
    }
    void setThreeLayersMode(bool enabled) noexcept {
        setMode(enabled ? dsp::Mode::III : dsp::Mode::II);
    }
    bool isThreeLayersMode() const noexcept {
        return isModeIII();
    }

    // ── Pitch & Gain Controls ───────────────────────────────────────────────
    void setLowPitch(float pitch) noexcept {
        if (auto* m = mixer()) m->setLowSemitones(pitch);
    }
    float getLowPitch() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->lowSemitones();
        return -6.0f;
    }
    void setLowGain(float gain) noexcept {
        if (auto* m = mixer()) m->setLowGain(gain);
    }
    float getLowGain() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->lowGain();
        return 0.75f;
    }

    void setHighPitch(float pitch) noexcept {
        if (auto* m = mixer()) m->setHighSemitones(pitch);
    }
    float getHighPitch() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->highSemitones();
        return 5.0f;
    }
    void setHighGain(float gain) noexcept {
        if (auto* m = mixer()) m->setHighGain(gain);
    }
    float getHighGain() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->highGain();
        return 0.40f;
    }

    void setDryGain(float gain) noexcept {
        if (auto* m = mixer()) m->setDryGain(gain);
    }
    float getDryGain() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->dryGain();
        return 0.50f;
    }

    void setMasterGain(float gain) noexcept {
        if (auto* m = mixer()) m->setMasterGain(gain);
    }
    float getMasterGain() const noexcept {
        if (auto* m = const_cast<AudioEngine*>(this)->mixer()) return m->masterGain();
        return 1.0f;
    }

    // ── Compatibility aliases ───────────────────────────────────────────────
    void setLayerAPitchSemitones(float pitch) noexcept { setLowPitch(pitch); }
    float getLayerAPitchSemitones() const noexcept { return getLowPitch(); }
    void setLayerAGainLinear(float gain) noexcept { setLowGain(gain); }
    float getLayerAGainLinear() const noexcept { return getLowGain(); }
    bool isLayerAEnabled() const noexcept { return getLowGain() > 0.0f; }
    void setLayerAEnabled(bool enabled) noexcept { if (!enabled) setLowGain(0.0f); }

    void setLayerBPitchSemitones(float pitch) noexcept { setHighPitch(pitch); }
    float getLayerBPitchSemitones() const noexcept { return getHighPitch(); }
    void setLayerBGainLinear(float gain) noexcept { setHighGain(gain); }
    float getLayerBGainLinear() const noexcept { return getHighGain(); }
    bool isLayerBEnabled() const noexcept { return getHighGain() > 0.0f; }
    void setLayerBEnabled(bool enabled) noexcept { if (!enabled) setHighGain(0.0f); }

    void setLayerMiddleGainLinear(float gain) noexcept { setDryGain(gain); }
    float getLayerMiddleGainLinear() const noexcept { return getDryGain(); }
    bool isLayerMiddleEnabled() const noexcept { return getDryGain() > 0.0f; }
    void setLayerMiddleEnabled(bool enabled) noexcept { if (!enabled) setDryGain(0.0f); }

public:
    struct Impl;

private:
    Impl* m_impl = nullptr;
};

} // namespace lawliet::audio
