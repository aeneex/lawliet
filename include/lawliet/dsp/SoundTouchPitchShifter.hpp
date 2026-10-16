#pragma once

#include "lawliet/dsp/IPitchShifter.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace soundtouch {
class SoundTouch;
}

namespace lawliet::dsp {

class SoundTouchPitchShifter final : public IPitchShifter {
public:
    SoundTouchPitchShifter();
    ~SoundTouchPitchShifter() override;

    void reset() override;
    void setSampleRate(std::uint32_t sr) override;
    void setPitchSemitones(float semitones) override;
    void process(const float* input, float* output, std::size_t frames) override;
    std::size_t latencyFrames() const override;

    float pitchSemitones() const noexcept { return m_pitchSemitones; }
    std::uint64_t underrunCount() const noexcept { return m_underruns.load(std::memory_order_relaxed); }

private:
    std::unique_ptr<soundtouch::SoundTouch> m_st;
    std::uint32_t m_sampleRate = 44100;
    float m_pitchSemitones = 0.0f;

    static constexpr std::size_t kFifoCapacity = 16384;
    std::vector<float> m_fifo;
    std::size_t m_fifoReadIdx  = 0;
    std::size_t m_fifoWriteIdx = 0;
    std::size_t m_fifoCount    = 0;
    std::atomic<std::uint64_t> m_underruns{0};
};

} // namespace lawliet::dsp
