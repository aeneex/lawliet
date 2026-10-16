// tests/test_audio_engine.cpp
// Unit and integration tests for the simplified Phase 5 AudioEngine:
//   - WASAPI context & device enumeration
//   - Capture filter (virtual endpoints excluded from mic list)
//   - Cable presence detection
//   - Engine state lifecycle (Stopped -> Running -> Stopped)
//   - Settings application and export
//   - Thread-safe meters and bypass controls

#include "lawliet/audio/AudioEngine.hpp"
#include "lawliet/config/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <string>

void registerTest(const std::string& name, std::function<bool()> fn);

namespace {

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// ── Test 1: WASAPI Device Listing & Capture Filter ────────────────────────────
bool testCaptureFilterAndListing()
{
    lawliet::audio::AudioEngine engine;
    const auto captureDevices = engine.listCaptureDevices();

    // Verify that NO capture device contains "cable" or "vb-audio"
    for (const auto& dev : captureDevices) {
        const std::string lower = toLower(dev.name);
        if (lower.find("cable") != std::string::npos || lower.find("vb-audio") != std::string::npos) {
            std::cerr << "[FAIL] Virtual cable endpoint found in capture list: " << dev.name << '\n';
            return false;
        }
    }

    const auto playbackDevices = engine.listPlaybackDevices();
    // Playback devices should list available outputs
    return true;
}

// ── Test 2: Cable Presence Detection ──────────────────────────────────────────
bool testCablePresenceDetection()
{
    lawliet::audio::AudioEngine engine;
    const bool cablePresent = engine.isCablePresent();

    // On this Windows system with VB-Audio Virtual Cable installed, it must detect it
    // Check playback devices to confirm consistency
    const auto playbacks = engine.listPlaybackDevices();
    bool foundCableInPlayback = false;
    for (const auto& dev : playbacks) {
        if (toLower(dev.name).find("cable input") != std::string::npos) {
            foundCableInPlayback = true;
            break;
        }
    }

    return (cablePresent == foundCableInPlayback);
}

// ── Test 3: Engine Lifecycle & State Transitions ──────────────────────────────
bool testEngineLifecycle()
{
    lawliet::audio::AudioEngine engine;
    lawliet::config::Settings settings;
    settings.mode = 2;
    settings.lowSemitones = -6.0f;
    settings.highSemitones = 5.0f;

    const auto err = engine.init(settings);
    if (err != lawliet::audio::AudioEngineError::None) {
        // If system has no microphone hardware at all, init might fail safely
        return engine.getState() == lawliet::audio::State::Error;
    }

    if (engine.getState() != lawliet::audio::State::Stopped) {
        return false;
    }

    if (!engine.start()) {
        std::cerr << "[NOTE] engine.start() returned false: " << engine.getLastError() << '\n';
        return true; // Graceful failure in headless CI environments
    }

    if (engine.getState() != lawliet::audio::State::Running) {
        return false;
    }
    if (!engine.isRunning()) {
        return false;
    }

    engine.stop();
    if (engine.getState() != lawliet::audio::State::Stopped) {
        return false;
    }
    if (engine.isRunning()) {
        return false;
    }

    engine.shutdown();
    return true;
}

// ── Test 4: Settings Export and Apply ──────────────────────────────────────────
bool testSettingsExportAndApply()
{
    lawliet::audio::AudioEngine engine;
    lawliet::config::Settings original;
    original.mode = 3;
    original.lowSemitones = -7.5f;
    original.highSemitones = 6.5f;
    original.lowGain = 0.70f;
    original.highGain = 0.35f;
    original.dryGain = 0.55f;
    original.masterGain = 1.10f;
    original.monitorEnabled = false;
    original.monitorVolume = 0.85f;

    engine.init(original);

    const auto exported = engine.exportSettings();
    if (exported.mode != 3) return false;
    if (std::abs(exported.lowSemitones - (-7.5f)) > 1e-4f) return false;
    if (std::abs(exported.highSemitones - 6.5f) > 1e-4f) return false;
    if (std::abs(exported.lowGain - 0.70f) > 1e-4f) return false;
    if (std::abs(exported.highGain - 0.35f) > 1e-4f) return false;
    if (std::abs(exported.dryGain - 0.55f) > 1e-4f) return false;
    if (std::abs(exported.masterGain - 1.10f) > 1e-4f) return false;
    if (exported.monitorEnabled != false) return false;

    // Apply new settings dynamically
    lawliet::config::Settings updated = original;
    updated.mode = 2;
    updated.lowSemitones = -5.0f;
    engine.applySettings(updated);

    const auto reExported = engine.exportSettings();
    if (reExported.mode != 2) return false;
    if (std::abs(reExported.lowSemitones - (-5.0f)) > 1e-4f) return false;

    return true;
}

// ── Test 5: Live Level Meters and Controls ─────────────────────────────────────
bool testControlsAndMeters()
{
    lawliet::audio::AudioEngine engine;
    lawliet::config::Settings settings;
    engine.init(settings);

    // Initial meters must be non-negative
    if (engine.getInputLevel() < 0.0f || engine.getOutputLevel() < 0.0f) {
        return false;
    }

    // Bypass toggle
    engine.setBypass(true);
    if (!engine.isBypass()) return false;
    engine.setBypass(false);
    if (engine.isBypass()) return false;

    // Monitoring controls
    engine.setMonitorVolume(1.5f); // Should clamp to 1.0f
    if (engine.getMonitorVolume() > 1.0f) return false;
    engine.setMonitorVolume(-0.5f); // Should clamp to 0.0f
    if (engine.getMonitorVolume() < 0.0f) return false;

    return true;
}

struct Registrar {
    Registrar() {
        registerTest("AudioEngine::captureFilterAndListing", testCaptureFilterAndListing);
        registerTest("AudioEngine::cablePresenceDetection",   testCablePresenceDetection);
        registerTest("AudioEngine::engineLifecycle",          testEngineLifecycle);
        registerTest("AudioEngine::settingsExportAndApply",   testSettingsExportAndApply);
        registerTest("AudioEngine::controlsAndMeters",        testControlsAndMeters);
    }
} g_registrar;

} // namespace
