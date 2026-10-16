// tests/test_dual_layer_mixer.cpp
// Unit tests for Phase 3 DualLayerMixer:
//   1. Mode II vs Mode III (Mode II has no dry, Mode III does)
//   2. Dry path latency alignment with SoundTouch shifters (impulse peak alignment)
//   3. Hard clipping limiter protection under extreme gains
//   4. Parameter clamping for out-of-range inputs

#include "lawliet/dsp/DualLayerMixer.hpp"
#include "lawliet/dsp/IPitchShifter.hpp"
#include "lawliet/dsp/SoundTouchPitchShifter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

void registerTest(const std::string& name, std::function<bool()> fn);

namespace {

// Stub: Passthrough shifter (zero latency, identity gain)
class PassthroughShifter final : public lawliet::dsp::IPitchShifter {
public:
    void reset() override {}
    void setSampleRate(std::uint32_t) override {}
    void setPitchSemitones(float) override {}
    void process(const float* in, float* out, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) out[i] = in[i];
    }
    std::size_t latencyFrames() const override { return 0; }
};

// Stub: Zero-output shifter
class ZeroShifter final : public lawliet::dsp::IPitchShifter {
public:
    void reset() override {}
    void setSampleRate(std::uint32_t) override {}
    void setPitchSemitones(float) override {}
    void process(const float*, float* out, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) out[i] = 0.0f;
    }
    std::size_t latencyFrames() const override { return 0; }
};

// ── Test 1: Mode II contains no dry component; Mode III does ───────────────────
bool testModeIINoDryModeIIIDoes()
{
    using namespace lawliet::dsp;
    // Zero-output shifters so any output must come from dry path
    DualLayerMixer mixer(
        std::make_unique<ZeroShifter>(),
        std::make_unique<ZeroShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setLowGain(0.0f);
    mixer.setHighGain(0.0f);
    mixer.setDryGain(1.0f);
    mixer.setMasterGain(1.0f);
    mixer.setDryDelayFrames(0); // 0 delay for direct test

    constexpr std::size_t N = 64;
    float in[N];
    float out[N] = {};
    for (std::size_t i = 0; i < N; ++i) in[i] = 0.5f;

    // Mode II: no dry component, output must be silence
    mixer.setMode(Mode::II);
    mixer.process(in, out, N);
    for (std::size_t i = 0; i < N; ++i) {
        if (out[i] != 0.0f) return false;
    }

    // Mode III: dry component is active and audible
    mixer.setMode(Mode::III);
    mixer.process(in, out, N);
    bool hasDrySignal = false;
    for (std::size_t i = 0; i < N; ++i) {
        if (std::abs(out[i] - 0.5f) < 1e-4f) {
            hasDrySignal = true;
            break;
        }
    }
    return hasDrySignal;
}

// ── Test 2: Dry path alignment with SoundTouch shifters ─────────────────────────
bool testDryPathAlignment()
{
    using namespace lawliet::dsp;
    // Real SoundTouch pitch shifters configured with tuned speech parameters
    DualLayerMixer mixer(
        std::make_unique<SoundTouchPitchShifter>(),
        std::make_unique<SoundTouchPitchShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setMode(Mode::III);
    mixer.setLowSemitones(0.0f); // 0 semitones
    mixer.setLowGain(0.5f);
    mixer.setHighGain(0.0f);     // high layer muted
    mixer.setDryGain(0.5f);      // dry layer active
    mixer.setMasterGain(1.0f);

    constexpr std::size_t blockSize = 256;
    constexpr std::size_t numBlocks = 16; // 4096 samples
    std::vector<float> in(blockSize, 0.0f);
    std::vector<float> out(blockSize, 0.0f);
    std::vector<float> fullOutput;

    in[0] = 1.0f; // Single impulse at frame 0

    for (std::size_t b = 0; b < numBlocks; ++b) {
        mixer.process(in.data(), out.data(), blockSize);
        fullOutput.insert(fullOutput.end(), out.begin(), out.end());
        in[0] = 0.0f;
    }

    std::size_t peakIdx = 0;
    float peakVal = 0.0f;
    for (std::size_t i = 0; i < fullOutput.size(); ++i) {
        if (std::abs(fullOutput[i]) > peakVal) {
            peakVal = std::abs(fullOutput[i]);
            peakIdx = i;
        }
    }

    const std::size_t expectedPeak = mixer.dryDelayFrames(); // 1024
    return (peakIdx == expectedPeak) && (peakVal > 0.4f);
}

// ── Test 3: Hard limiter protects output from exceeding ±1.0 ──────────────────
bool testHardLimiterExtremeGains()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setMode(Mode::III);
    mixer.setLowGain(1.0f);
    mixer.setHighGain(1.0f);
    mixer.setDryGain(1.0f);
    mixer.setMasterGain(2.0f); // (1 + 1 + 1) * 2 = 6x gain

    constexpr std::size_t N = 128;
    float in[N];
    float out[N];

    // Standard ±1.0 full-scale input
    for (std::size_t i = 0; i < N; ++i) {
        in[i] = (i % 2 == 0) ? 1.0f : -1.0f;
    }
    mixer.process(in, out, N);
    for (std::size_t i = 0; i < N; ++i) {
        if (out[i] > 1.0f || out[i] < -1.0f) return false;
    }

    // Extreme input signal (±100.0)
    for (std::size_t i = 0; i < N; ++i) {
        in[i] = (i % 2 == 0) ? 100.0f : -100.0f;
    }
    mixer.process(in, out, N);
    for (std::size_t i = 0; i < N; ++i) {
        if (out[i] > 1.0f || out[i] < -1.0f) return false;
    }

    return true;
}

// ── Test 4: Parameter clamping for out-of-range inputs ─────────────────────────
bool testParameterClamping()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer;

    // lowSemitones: -12 to 0
    mixer.setLowSemitones(-50.0f);
    if (mixer.lowSemitones() != -12.0f) return false;
    mixer.setLowSemitones(10.0f);
    if (mixer.lowSemitones() != 0.0f) return false;
    mixer.setLowSemitones(-6.0f);
    if (std::abs(mixer.lowSemitones() - (-6.0f)) > 1e-4f) return false;

    // highSemitones: 0 to +12
    mixer.setHighSemitones(-5.0f);
    if (mixer.highSemitones() != 0.0f) return false;
    mixer.setHighSemitones(50.0f);
    if (mixer.highSemitones() != 12.0f) return false;
    mixer.setHighSemitones(5.0f);
    if (std::abs(mixer.highSemitones() - 5.0f) > 1e-4f) return false;

    // lowGain: 0 to 1
    mixer.setLowGain(-2.0f);
    if (mixer.lowGain() != 0.0f) return false;
    mixer.setLowGain(3.0f);
    if (mixer.lowGain() != 1.0f) return false;

    // highGain: 0 to 1
    mixer.setHighGain(-1.0f);
    if (mixer.highGain() != 0.0f) return false;
    mixer.setHighGain(2.0f);
    if (mixer.highGain() != 1.0f) return false;

    // dryGain: 0 to 1
    mixer.setDryGain(-1.0f);
    if (mixer.dryGain() != 0.0f) return false;
    mixer.setDryGain(2.0f);
    if (mixer.dryGain() != 1.0f) return false;

    // masterGain: 0 to 2
    mixer.setMasterGain(-1.0f);
    if (mixer.masterGain() != 0.0f) return false;
    mixer.setMasterGain(10.0f);
    if (mixer.masterGain() != 2.0f) return false;

    // mode: II or III
    mixer.setMode(1);
    if (mixer.mode() != Mode::II) return false;
    mixer.setMode(99);
    if (mixer.mode() != Mode::III) return false;

    return true;
}

struct Registrar {
    Registrar() {
        registerTest("DualLayerMixer::modeIINoDryModeIIIDoes", testModeIINoDryModeIIIDoes);
        registerTest("DualLayerMixer::dryPathAlignment",        testDryPathAlignment);
        registerTest("DualLayerMixer::hardLimiterExtremeGains", testHardLimiterExtremeGains);
        registerTest("DualLayerMixer::parameterClamping",       testParameterClamping);
    }
} g_registrar;

} // namespace
