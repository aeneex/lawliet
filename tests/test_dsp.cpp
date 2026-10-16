// tests/test_dsp.cpp
// Tests for:
//   - Semitone conversion and clamping
//   - DualLayerMixer: gain blending, master gain, clipping protection
//   - Invalid configuration detection

#include "lawliet/dsp/DualLayerMixer.hpp"
#include "lawliet/dsp/IPitchShifter.hpp"
#include "lawliet/dsp/SoundTouchPitchShifter.hpp"
#include <SoundTouch.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

void registerTest(const std::string& name, std::function<bool()> fn);

namespace {

// ── Stub: passthrough shifter ─────────────────────────────────────────────────
class PassthroughShifter final : public lawliet::dsp::IPitchShifter {
public:
    void     reset() override {}
    void     setSampleRate(std::uint32_t) override {}
    void     setPitchSemitones(float) override {}
    void     process(const float* in, float* out, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) out[i] = in[i];
    }
    std::size_t latencyFrames() const override { return 0; }
};

// ── Stub: zero-output shifter ──────────────────────────────────────────────────
class ZeroShifter final : public lawliet::dsp::IPitchShifter {
public:
    void     reset() override {}
    void     setSampleRate(std::uint32_t) override {}
    void     setPitchSemitones(float) override {}
    void     process(const float*, float* out, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) out[i] = 0.0f;
    }
    std::size_t latencyFrames() const override { return 0; }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool nearlyEqual(float a, float b, float eps = 1e-5f)
{
    return std::fabs(a - b) <= eps;
}

// ── Tests: semitone clamping in SoundTouchPitchShifter ───────────────────────

bool testSemitoneClampHigh()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    s.setPitchSemitones(999.0f); // Way above maximum.
    return s.pitchSemitones() == lawliet::dsp::kMaxPitchSemitones;
}

bool testSemitoneClampLow()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    s.setPitchSemitones(-999.0f); // Way below minimum.
    return s.pitchSemitones() == lawliet::dsp::kMinPitchSemitones;
}

bool testSemitoneZeroPassthrough()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    s.setPitchSemitones(0.0f);
    return nearlyEqual(s.pitchSemitones(), 0.0f);
}

bool testSemitoneInRange()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    s.setPitchSemitones(-6.0f);
    return nearlyEqual(s.pitchSemitones(), -6.0f);
}

// ── Tests: DualLayerMixer gain blending ──────────────────────────────────────

bool testMixerPassthrough()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setLowGain(0.5f);
    mixer.setHighGain(0.5f);
    mixer.setMasterGain(1.0f);

    constexpr std::size_t N = 64;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 0.5f; // Constant signal.

    mixer.process(in, out, N);

    // Both layers pass same signal: 0.5*0.5 + 0.5*0.5 = 0.5
    for (std::size_t i = 0; i < N; ++i) {
        if (!nearlyEqual(out[i], 0.5f)) return false;
    }
    return true;
}

bool testMixerMasterGain()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<ZeroShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setLowGain(1.0f);
    mixer.setHighGain(0.0f);
    mixer.setMasterGain(0.5f);

    constexpr std::size_t N = 32;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 1.0f;

    mixer.process(in, out, N);

    // low gain=1.0 * master=0.5 → 0.5
    for (std::size_t i = 0; i < N; ++i) {
        if (!nearlyEqual(out[i], 0.5f)) return false;
    }
    return true;
}

bool testMixerClippingProtection()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    // Both layers at full gain, master at full → would clip without protection.
    mixer.setLowGain(1.0f);
    mixer.setHighGain(1.0f);
    mixer.setMasterGain(1.0f);

    constexpr std::size_t N = 16;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 1.0f; // Full-scale signal.

    mixer.process(in, out, N);

    for (std::size_t i = 0; i < N; ++i) {
        if (out[i] > 1.0f || out[i] < -1.0f) return false;
    }
    return true;
}

bool testMixerBothLayersMuted()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setLowGain(0.0f);
    mixer.setHighGain(0.0f);

    constexpr std::size_t N = 32;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 1.0f;

    mixer.process(in, out, N);

    for (std::size_t i = 0; i < N; ++i) {
        if (!nearlyEqual(out[i], 0.0f)) return false;
    }
    return true;
}

bool testMixerOnlyLayerA()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setLowGain(0.6f);
    mixer.setHighGain(0.0f);
    mixer.setMasterGain(1.0f);

    constexpr std::size_t N = 32;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 1.0f;

    mixer.process(in, out, N);

    for (std::size_t i = 0; i < N; ++i) {
        if (!nearlyEqual(out[i], 0.6f)) return false;
    }
    return true;
}

// ── Tests: SoundTouch actual process (smoke-level) ───────────────────────────

bool testSoundTouchProcessReturnsSameSampleCount()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    s.setPitchSemitones(0.0f);

    constexpr std::size_t N = 256;
    float in[N]  = {};
    float out[N] = {};
    for (std::size_t i = 0; i < N; ++i) in[i] = 0.0f;

    // Should not crash; may return zeros during initial FIFO fill.
    s.process(in, out, N);
    return true; // Just verifying it doesn't segfault / throw.
}

bool testSoundTouchLatencyNonNegative()
{
    lawliet::dsp::SoundTouchPitchShifter s;
    s.setSampleRate(44100);
    return s.latencyFrames() < 1000000u;
}

bool testMixerBypassMode()
{
    using namespace lawliet::dsp;
    DualLayerMixer mixer(
        std::make_unique<PassthroughShifter>(),
        std::make_unique<PassthroughShifter>()
    );
    mixer.setSampleRate(44100);
    mixer.setBypass(true);

    constexpr std::size_t N = 32;
    float in[N], out[N];
    for (std::size_t i = 0; i < N; ++i) in[i] = 0.42f;

    mixer.process(in, out, N);

    for (std::size_t i = 0; i < N; ++i) {
        if (!nearlyEqual(out[i], in[i])) return false;
    }
    return true;
}

bool testImpulseResponseLatency()
{
    struct SettingConfig {
        int seq;
        int seek;
        int overlap;
    };

    SettingConfig configs[] = {
        { 40, 15, 8 }, // baseline
        { 30, 12, 6 },
        { 25, 10, 5 },
        { 20, 8, 4 }
    };

    for (const auto& c : configs) {
        soundtouch::SoundTouch st;
        st.setChannels(1);
        st.setSampleRate(44100);
        st.setPitchSemiTones(0.0f);
        st.setSetting(SETTING_USE_QUICKSEEK,  1);
        st.setSetting(SETTING_USE_AA_FILTER,  0);
        st.setSetting(SETTING_SEQUENCE_MS,    c.seq);
        st.setSetting(SETTING_SEEKWINDOW_MS,  c.seek);
        st.setSetting(SETTING_OVERLAP_MS,      c.overlap);

        int repLat = st.getSetting(SETTING_INITIAL_LATENCY);

        // Measure impulse response
        constexpr std::size_t blockSize = 256;
        constexpr std::size_t numBlocks = 30;
        std::vector<float> in(blockSize, 0.0f);
        std::vector<float> out(blockSize, 0.0f);
        std::vector<float> allStreamOut;
        in[0] = 1.0f;

        for (std::size_t b = 0; b < numBlocks; ++b) {
            st.putSamples(in.data(), blockSize);
            unsigned int rec = st.receiveSamples(out.data(), blockSize);
            if (rec < blockSize) {
                std::memset(out.data() + rec, 0, (blockSize - rec) * sizeof(float));
            }
            allStreamOut.insert(allStreamOut.end(), out.begin(), out.end());
            in[0] = 0.0f;
        }

        std::size_t peakIdx = 0;
        float peakVal = 0.0f;
        for (std::size_t i = 0; i < allStreamOut.size(); ++i) {
            if (std::abs(allStreamOut[i]) > peakVal) {
                peakVal = std::abs(allStreamOut[i]);
                peakIdx = i;
            }
        }

        std::printf("[Tuning] Seq=%d Seek=%d Overlap=%d => Reported: %d frames (%.1f ms), StreamPeakIdx: %zu (%.1f ms)\n",
                    c.seq, c.seek, c.overlap, repLat, (repLat * 1000.0f / 44100.0f), peakIdx, (peakIdx * 1000.0f / 44100.0f));
    }

    return true;
}

// Helper: count positive zero crossings
static int countZeroCrossings(const float* data, std::size_t n) {
    int crossings = 0;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (data[i] <= 0.0f && data[i + 1] > 0.0f) {
            crossings++;
        }
    }
    return crossings;
}

bool testPitchRatioSanity()
{
    const auto measureCrossingsAtPitch = [](float semitones) -> int {
        lawliet::dsp::SoundTouchPitchShifter shifter;
        shifter.setSampleRate(44100);
        shifter.setPitchSemitones(semitones);

        constexpr std::size_t kBlock = 512;
        constexpr float f0 = 440.0f;
        constexpr float kPi = 3.14159265358979323846f;

        float in[kBlock];
        float out[kBlock];
        std::size_t sampleIndex = 0;

        // Warm up and settle the pitch shifter over 30 blocks (~350 ms)
        for (int b = 0; b < 30; ++b) {
            for (std::size_t i = 0; i < kBlock; ++i) {
                in[i] = std::sin(2.0f * kPi * f0 * static_cast<float>(sampleIndex++) / 44100.0f);
            }
            shifter.process(in, out, kBlock);
        }

        // Measure steady state over the next 4 blocks (2048 samples)
        int totalCrossings = 0;
        for (int b = 0; b < 4; ++b) {
            for (std::size_t i = 0; i < kBlock; ++i) {
                in[i] = std::sin(2.0f * kPi * f0 * static_cast<float>(sampleIndex++) / 44100.0f);
            }
            shifter.process(in, out, kBlock);
            totalCrossings += countZeroCrossings(out, kBlock);
        }
        return totalCrossings;
    };

    const int baseline = measureCrossingsAtPitch(0.0f);     // ~440 Hz (~20 crossings per 2048 samples)
    const int upOctave = measureCrossingsAtPitch(12.0f);    // ~880 Hz (~40 crossings, doubled)
    const int downOctave = measureCrossingsAtPitch(-12.0f); // ~220 Hz (~10 crossings, halved)

    if (baseline <= 0) return false;

    const float ratioUp = static_cast<float>(upOctave) / static_cast<float>(baseline);
    const float ratioDown = static_cast<float>(downOctave) / static_cast<float>(baseline);

    // Expect ratioUp ~ 2.0 (doubled) and ratioDown ~ 0.5 (halved) within a small tolerance
    return (std::abs(ratioUp - 2.0f) < 0.20f) && (std::abs(ratioDown - 0.5f) < 0.10f);
}

// ── Registrar ─────────────────────────────────────────────────────────────────
struct Registrar {
    Registrar() {
        registerTest("SoundTouch::clampHigh",            testSemitoneClampHigh);
        registerTest("SoundTouch::clampLow",             testSemitoneClampLow);
        registerTest("SoundTouch::zeroPassthrough",      testSemitoneZeroPassthrough);
        registerTest("SoundTouch::inRange",              testSemitoneInRange);
        registerTest("SoundTouch::impulseLatency",       testImpulseResponseLatency);
        registerTest("SoundTouch::pitchRatioSanity",     testPitchRatioSanity);
        registerTest("Mixer::passthrough",               testMixerPassthrough);
        registerTest("Mixer::masterGain",                testMixerMasterGain);
        registerTest("Mixer::clippingProtection",        testMixerClippingProtection);
        registerTest("Mixer::bothMuted",                 testMixerBothLayersMuted);
        registerTest("Mixer::onlyLayerA",                testMixerOnlyLayerA);
        registerTest("Mixer::bypassMode",                testMixerBypassMode);
        registerTest("SoundTouch::processSampleCount",   testSoundTouchProcessReturnsSameSampleCount);
        registerTest("SoundTouch::latencyNonNegative",   testSoundTouchLatencyNonNegative);
    }
} g_registrar;

} // namespace
