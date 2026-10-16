// src/dsp/SoundTouchPitchShifter.cpp
// Concrete IPitchShifter backed by SoundTouch.
// SoundTouch uses a FIFO model: we push input samples, then pull output samples.
// We handle the latency fill transparently so the caller always gets frameCount
// output samples even during the initial FIFO fill phase.

#include "lawliet/dsp/SoundTouchPitchShifter.hpp"

// SoundTouch public C++ header (fetched via CMake FetchContent).
#include <SoundTouch.h>

#include <algorithm>
#include <cassert>

#include <cstring>
#include <vector>

namespace lawliet::dsp {

// ── Helpers ───────────────────────────────────────────────────────────────────

/// Clamp semitones to the allowed range.
static float clampSemitones(float s)
{
    return std::clamp(s, kMinPitchSemitones, kMaxPitchSemitones);
}



// ── SoundTouchPitchShifter ────────────────────────────────────────────────────

SoundTouchPitchShifter::SoundTouchPitchShifter()
    : m_st(std::make_unique<soundtouch::SoundTouch>())
    , m_fifo(kFifoCapacity, 0.0f)
{
    // Configure for mono real-time speech use.
    m_st->setChannels(1);
    m_st->setSampleRate(m_sampleRate);
    m_st->setPitchSemiTones(0.0f);

    // SoundTouch settings tuned for low-latency speech (Phase 3):
    // Small sequence, seek, and overlap to achieve <25ms latency with clean voice timbre.
    m_st->setSetting(SETTING_USE_QUICKSEEK,        1);
    m_st->setSetting(SETTING_USE_AA_FILTER,        0); // disable AA for speed and low delay
    m_st->setSetting(SETTING_SEQUENCE_MS,         20); // 20 ms processing sequence
    m_st->setSetting(SETTING_SEEKWINDOW_MS,        8); // 8 ms seek window
    m_st->setSetting(SETTING_OVERLAP_MS,           4); // 4 ms overlap window
}

SoundTouchPitchShifter::~SoundTouchPitchShifter() = default;

void SoundTouchPitchShifter::reset()
{
    m_st->clear();
    m_fifoReadIdx  = 0;
    m_fifoWriteIdx = 0;
    m_fifoCount    = 0;
    m_underruns.store(0, std::memory_order_relaxed);
}

void SoundTouchPitchShifter::setSampleRate(std::uint32_t sr)
{
    if (sr == m_sampleRate) return;
    m_sampleRate = sr;
    m_st->setSampleRate(sr);
    reset();
}

void SoundTouchPitchShifter::setPitchSemitones(float semitones)
{
    semitones = clampSemitones(semitones);
    if (semitones == m_pitchSemitones) return;
    m_pitchSemitones = semitones;
    m_st->setPitchSemiTones(semitones);
}

void SoundTouchPitchShifter::process(const float* input,
                                      float*       output,
                                      std::size_t  frames)
{
    assert(frames > 0);

    // 1. Push input samples into SoundTouch pipeline.
    m_st->putSamples(input, static_cast<unsigned int>(frames));

    // 2. Drain all available processed samples into the preallocated layer FIFO.
    float tempBuf[512];
    unsigned int rec = 0;
    while ((rec = m_st->receiveSamples(tempBuf, 512)) > 0) {
        for (unsigned int i = 0; i < rec; ++i) {
            if (m_fifoCount < kFifoCapacity) {
                m_fifo[m_fifoWriteIdx] = tempBuf[i];
                m_fifoWriteIdx = (m_fifoWriteIdx + 1) % kFifoCapacity;
                ++m_fifoCount;
            } else {
                // Drop oldest sample to prevent latency creep under overflow
                m_fifoReadIdx = (m_fifoReadIdx + 1) % kFifoCapacity;
                m_fifo[m_fifoWriteIdx] = tempBuf[i];
                m_fifoWriteIdx = (m_fifoWriteIdx + 1) % kFifoCapacity;
            }
        }
    }

    // 3. Pull exactly N frames from FIFO; if short, pad with zeros and bump underruns.
    const std::size_t toPull = std::min(m_fifoCount, frames);
    for (std::size_t i = 0; i < toPull; ++i) {
        output[i] = m_fifo[m_fifoReadIdx];
        m_fifoReadIdx = (m_fifoReadIdx + 1) % kFifoCapacity;
    }
    m_fifoCount -= toPull;

    if (toPull < frames) {
        std::memset(output + toPull, 0, (frames - toPull) * sizeof(float));
        m_underruns.fetch_add(1, std::memory_order_relaxed);
    }
}

std::size_t SoundTouchPitchShifter::latencyFrames() const
{
    // Empirically measured peak latency for tuned speech settings (20ms/8ms/4ms):
    // 1024 frames @ 44.1 kHz (~23.2 ms).
    return (1024u * m_sampleRate) / 44100u;
}

} // namespace lawliet::dsp
