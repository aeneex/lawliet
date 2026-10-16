// src/dsp/DualLayerMixer.cpp
// Reworked DualLayerMixer supporting Mode II and Mode III with delay-aligned dry path,
// per-sample gain smoothing, and hard limiter.

#include "lawliet/dsp/DualLayerMixer.hpp"
#include "lawliet/dsp/SoundTouchPitchShifter.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace lawliet::dsp {

DualLayerMixer::DualLayerMixer()
    : DualLayerMixer(std::make_unique<SoundTouchPitchShifter>(),
                     std::make_unique<SoundTouchPitchShifter>())
{
}

DualLayerMixer::DualLayerMixer(std::unique_ptr<IPitchShifter> shifterLow,
                               std::unique_ptr<IPitchShifter> shifterHigh)
    : m_shifterLow(std::move(shifterLow))
    , m_shifterHigh(std::move(shifterHigh))
    , m_dryDelayLine(kMaxDelayFrames, 0.0f)
{
    if (!m_shifterLow || !m_shifterHigh) {
        throw std::invalid_argument("DualLayerMixer: shifters must not be null");
    }

    m_dryDelayFrames = m_shifterLow->latencyFrames();
    m_shifterLow->setPitchSemitones(m_currentLowPitch);
    m_shifterHigh->setPitchSemitones(m_currentHighPitch);
}

void DualLayerMixer::setSampleRate(std::uint32_t sampleRate)
{
    m_sampleRate = sampleRate ? sampleRate : 44100;
    m_shifterLow->setSampleRate(m_sampleRate);
    m_shifterHigh->setSampleRate(m_sampleRate);

    m_dryDelayFrames = m_shifterLow->latencyFrames();
    m_gainSmoothAlpha = 1.0f - std::exp(-1.0f / (0.010f * static_cast<float>(m_sampleRate)));

    std::fill(m_dryDelayLine.begin(), m_dryDelayLine.end(), 0.0f);
    m_dryWriteIdx = 0;
}

// ── Mode ────────────────────────────────────────────────────────────────────

void DualLayerMixer::setMode(Mode m) noexcept
{
    m_mode.store(std::clamp(static_cast<int>(m), 2, 3), std::memory_order_relaxed);
}

void DualLayerMixer::setMode(int m) noexcept
{
    m_mode.store(std::clamp(m, 2, 3), std::memory_order_relaxed);
}

Mode DualLayerMixer::mode() const noexcept
{
    return (m_mode.load(std::memory_order_relaxed) == 3) ? Mode::III : Mode::II;
}

bool DualLayerMixer::isModeIII() const noexcept
{
    return m_mode.load(std::memory_order_relaxed) == 3;
}

// ── Parameters (Clamped) ────────────────────────────────────────────────────

void DualLayerMixer::setLowSemitones(float st) noexcept
{
    m_lowSemitones.store(std::clamp(st, -12.0f, 0.0f), std::memory_order_relaxed);
}

float DualLayerMixer::lowSemitones() const noexcept
{
    return std::clamp(m_lowSemitones.load(std::memory_order_relaxed), -12.0f, 0.0f);
}

void DualLayerMixer::setHighSemitones(float st) noexcept
{
    m_highSemitones.store(std::clamp(st, 0.0f, 12.0f), std::memory_order_relaxed);
}

float DualLayerMixer::highSemitones() const noexcept
{
    return std::clamp(m_highSemitones.load(std::memory_order_relaxed), 0.0f, 12.0f);
}

void DualLayerMixer::setLowGain(float g) noexcept
{
    g = std::clamp(g, 0.0f, 1.0f);
    m_lowGain.store(g, std::memory_order_relaxed);
    if (!m_processingStarted) m_smoothedLowGain = g;
}

float DualLayerMixer::lowGain() const noexcept
{
    return std::clamp(m_lowGain.load(std::memory_order_relaxed), 0.0f, 1.0f);
}

void DualLayerMixer::setHighGain(float g) noexcept
{
    g = std::clamp(g, 0.0f, 1.0f);
    m_highGain.store(g, std::memory_order_relaxed);
    if (!m_processingStarted) m_smoothedHighGain = g;
}

float DualLayerMixer::highGain() const noexcept
{
    return std::clamp(m_highGain.load(std::memory_order_relaxed), 0.0f, 1.0f);
}

void DualLayerMixer::setDryGain(float g) noexcept
{
    g = std::clamp(g, 0.0f, 1.0f);
    m_dryGain.store(g, std::memory_order_relaxed);
    if (!m_processingStarted) m_smoothedDryGain = g;
}

float DualLayerMixer::dryGain() const noexcept
{
    return std::clamp(m_dryGain.load(std::memory_order_relaxed), 0.0f, 1.0f);
}

void DualLayerMixer::setMasterGain(float g) noexcept
{
    g = std::clamp(g, 0.0f, 2.0f);
    m_masterGain.store(g, std::memory_order_relaxed);
    if (!m_processingStarted) m_smoothedMasterGain = g;
}

float DualLayerMixer::masterGain() const noexcept
{
    return std::clamp(m_masterGain.load(std::memory_order_relaxed), 0.0f, 2.0f);
}

void DualLayerMixer::setBypass(bool bypass) noexcept
{
    m_bypass.store(bypass, std::memory_order_relaxed);
}

bool DualLayerMixer::isBypass() const noexcept
{
    return m_bypass.load(std::memory_order_relaxed);
}

std::uint64_t DualLayerMixer::underrunCount() const noexcept
{
    std::uint64_t count = 0;
    if (auto* stLow = dynamic_cast<SoundTouchPitchShifter*>(m_shifterLow.get())) {
        count += stLow->underrunCount();
    }
    if (auto* stHigh = dynamic_cast<SoundTouchPitchShifter*>(m_shifterHigh.get())) {
        count += stHigh->underrunCount();
    }
    return count;
}

// ── Process ─────────────────────────────────────────────────────────────────

void DualLayerMixer::process(const float* input, float* output, std::size_t frames)
{
    assert(frames <= kMaxFrames && "DualLayerMixer: frame count exceeds kMaxFrames");

    if (m_bypass.load(std::memory_order_relaxed)) {
        std::memcpy(output, input, frames * sizeof(float));
        return;
    }

    const Mode curMode = mode();
    const float targetLowPitch   = lowSemitones();
    const float targetHighPitch  = highSemitones();
    const float targetLowGain    = lowGain();
    const float targetHighGain   = highGain();
    const float targetDryGain    = dryGain();
    const float targetMasterGain = masterGain();

    // Update pitch shifters only when target moved by >= 0.01 semitones
    if (std::abs(targetLowPitch - m_currentLowPitch) >= 0.01f) {
        m_shifterLow->setPitchSemitones(targetLowPitch);
        m_currentLowPitch = targetLowPitch;
    }
    if (std::abs(targetHighPitch - m_currentHighPitch) >= 0.01f) {
        m_shifterHigh->setPitchSemitones(targetHighPitch);
        m_currentHighPitch = targetHighPitch;
    }

    if (!m_processingStarted) {
        m_smoothedLowGain    = targetLowGain;
        m_smoothedHighGain   = targetHighGain;
        m_smoothedDryGain    = targetDryGain;
        m_smoothedMasterGain = targetMasterGain;
        m_processingStarted  = true;
    }

    // Process both pitch shifters
    m_shifterLow->process(input, m_bufLow.data(), frames);
    m_shifterHigh->process(input, m_bufHigh.data(), frames);

    const float alpha = m_gainSmoothAlpha;
    const std::size_t delay = m_dryDelayFrames;
    const std::size_t cap = m_dryDelayLine.size();

    for (std::size_t i = 0; i < frames; ++i) {
        // Read from dry delay line
        m_dryDelayLine[m_dryWriteIdx] = input[i];
        const std::size_t readIdx = (m_dryWriteIdx + cap - delay) % cap;
        const float drySample = m_dryDelayLine[readIdx];
        m_dryWriteIdx = (m_dryWriteIdx + 1) % cap;

        // Smooth gains per sample (~10ms ramp)
        m_smoothedLowGain    += alpha * (targetLowGain    - m_smoothedLowGain);
        m_smoothedHighGain   += alpha * (targetHighGain   - m_smoothedHighGain);
        m_smoothedDryGain    += alpha * (targetDryGain    - m_smoothedDryGain);
        m_smoothedMasterGain += alpha * (targetMasterGain - m_smoothedMasterGain);

        float s = (m_bufLow[i] * m_smoothedLowGain) + (m_bufHigh[i] * m_smoothedHighGain);
        if (curMode == Mode::III) {
            s += drySample * m_smoothedDryGain;
        }

        s *= m_smoothedMasterGain;

        // Hard limiter
        output[i] = std::clamp(s, -1.0f, 1.0f);
    }
}

} // namespace lawliet::dsp
