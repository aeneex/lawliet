#pragma once

#include <cstddef>
#include <cstdint>

namespace lawliet::dsp {

constexpr float kMinPitchSemitones = -12.0f;
constexpr float kMaxPitchSemitones =  12.0f;

class IPitchShifter {
public:
    virtual ~IPitchShifter() = default;
    virtual void reset() = 0;
    virtual void setSampleRate(std::uint32_t sampleRate) = 0;
    virtual void setPitchSemitones(float semitones) = 0;
    virtual void process(const float* in, float* out, std::size_t frames) = 0;
    virtual std::size_t latencyFrames() const = 0;
};

} // namespace lawliet::dsp
