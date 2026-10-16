#pragma once

#include "lawliet/dsp/IPitchShifter.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace lawliet::dsp {

enum class Mode {
    II  = 2,
    III = 3
};

class DualLayerMixer {
public:
    static constexpr std::size_t kMaxFrames = 4096;
    static constexpr std::size_t kMaxDelayFrames = 16384;

    DualLayerMixer();
    DualLayerMixer(std::unique_ptr<IPitchShifter> shifterLow,
                   std::unique_ptr<IPitchShifter> shifterHigh);
    ~DualLayerMixer() = default;

    void setSampleRate(std::uint32_t sampleRate);
    void process(const float* input, float* output, std::size_t frames);

    // ── Mode ────────────────────────────────────────────────────────────────
    void setMode(Mode m) noexcept;
    void setMode(int m) noexcept;
    Mode mode() const noexcept;
    bool isModeIII() const noexcept;

    // ── Pitch & Gain parameters ─────────────────────────────────────────────
    void setLowSemitones(float st) noexcept;
    float lowSemitones() const noexcept;

    void setHighSemitones(float st) noexcept;
    float highSemitones() const noexcept;

    void setLowGain(float g) noexcept;
    float lowGain() const noexcept;

    void setHighGain(float g) noexcept;
    float highGain() const noexcept;

    void setDryGain(float g) noexcept;
    float dryGain() const noexcept;

    void setMasterGain(float g) noexcept;
    float masterGain() const noexcept;

    // ── Bypass ──────────────────────────────────────────────────────────────
    void setBypass(bool bypass) noexcept;
    bool isBypass() const noexcept;

    // ── Inspection / Diagnostics ────────────────────────────────────────────
    IPitchShifter* lowShifter() noexcept { return m_shifterLow.get(); }
    IPitchShifter* highShifter() noexcept { return m_shifterHigh.get(); }

    std::size_t dryDelayFrames() const noexcept { return m_dryDelayFrames; }
    void setDryDelayFrames(std::size_t delay) noexcept { m_dryDelayFrames = delay; }
    std::uint64_t underrunCount() const noexcept;

private:
    std::unique_ptr<IPitchShifter> m_shifterLow;
    std::unique_ptr<IPitchShifter> m_shifterHigh;

    std::uint32_t m_sampleRate = 44100;
    float m_gainSmoothAlpha = 0.00226f;

    // Thread-safe parameters written by GUI / config, read by audio thread
    std::atomic<int>   m_mode{static_cast<int>(Mode::II)};
    std::atomic<float> m_lowSemitones{-6.0f};
    std::atomic<float> m_highSemitones{5.0f};
    std::atomic<float> m_lowGain{0.75f};
    std::atomic<float> m_highGain{0.40f};
    std::atomic<float> m_dryGain{0.50f};
    std::atomic<float> m_masterGain{1.0f};
    std::atomic<bool>  m_bypass{false};

    // Audio thread cached state for smoothing & pitch updates
    float m_currentLowPitch    = -6.0f;
    float m_currentHighPitch   = 5.0f;
    float m_smoothedLowGain    = 0.75f;
    float m_smoothedHighGain   = 0.40f;
    float m_smoothedDryGain    = 0.50f;
    float m_smoothedMasterGain = 1.0f;
    bool  m_processingStarted  = false;

    // Preallocated dry delay line
    std::vector<float> m_dryDelayLine;
    std::size_t m_dryWriteIdx = 0;
    std::size_t m_dryDelayFrames = 0;

    // Scratch buffers for shifters
    std::array<float, kMaxFrames> m_bufLow{};
    std::array<float, kMaxFrames> m_bufHigh{};
};

} // namespace lawliet::dsp
