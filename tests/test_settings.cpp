// tests/test_settings.cpp
// Unit tests for the INI Settings layer:
//   - Round trip save/load
//   - Missing file handling
//   - Garbage file handling
//   - Out-of-range value clamping
//   - Unknown keys and comments handling
//   - Device names with spaces and non-ASCII UTF-8 characters

#include "lawliet/config/Settings.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

void registerTest(const std::string& name, std::function<bool()> fn);

namespace {

static bool nearlyEqual(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

// ── Test 1: Round trip save and load ──────────────────────────────────────────
bool testRoundTrip()
{
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "lawliet_test_settings_rt";
    fs::create_directories(tmpDir);
    const fs::path iniPath = tmpDir / "test.ini";

    lawliet::config::Settings original;
    original.mode           = 3;
    original.lowSemitones   = -8.0f;
    original.highSemitones  = 7.0f;
    original.lowGain        = 0.65f;
    original.highGain       = 0.35f;
    original.dryGain        = 0.45f;
    original.masterGain     = 1.25f;
    original.inputDevice    = "USB Audio Microphone";
    original.outputDevice   = "CABLE Input (VB-Audio Virtual Cable)";
    original.monitorDevice  = "Realtek High Definition Audio";
    original.monitorEnabled = true;
    original.monitorVolume  = 0.90f;

    if (!lawliet::config::saveSettings(iniPath, original)) {
        fs::remove_all(tmpDir);
        return false;
    }

    lawliet::config::Settings loaded;
    if (!lawliet::config::loadSettings(iniPath, loaded)) {
        fs::remove_all(tmpDir);
        return false;
    }

    fs::remove_all(tmpDir);

    return (loaded.mode == 3)
        && nearlyEqual(loaded.lowSemitones,  -8.0f)
        && nearlyEqual(loaded.highSemitones,  7.0f)
        && nearlyEqual(loaded.lowGain,        0.65f)
        && nearlyEqual(loaded.highGain,       0.35f)
        && nearlyEqual(loaded.dryGain,        0.45f)
        && nearlyEqual(loaded.masterGain,     1.25f)
        && (loaded.inputDevice    == original.inputDevice)
        && (loaded.outputDevice   == original.outputDevice)
        && (loaded.monitorDevice  == original.monitorDevice)
        && (loaded.monitorEnabled == true)
        && nearlyEqual(loaded.monitorVolume,  0.90f);
}

// ── Test 2: Missing file defaults ─────────────────────────────────────────────
bool testMissingFile()
{
    namespace fs = std::filesystem;
    const fs::path nonExistent = fs::temp_directory_path() / "lawliet_definitely_missing_file_xyz.ini";
    fs::remove(nonExistent);

    lawliet::config::Settings s;
    s.mode = 99; // Corrupt to verify it resets to defaults
    s.lowSemitones = -100.0f;

    const bool res = lawliet::config::loadSettings(nonExistent, s);

    // Missing file returns false, but resets struct to valid default settings
    return (!res)
        && (s.mode == 2)
        && nearlyEqual(s.lowSemitones,  -6.0f)
        && nearlyEqual(s.highSemitones,  5.0f)
        && nearlyEqual(s.lowGain,        0.75f)
        && nearlyEqual(s.highGain,       0.40f)
        && nearlyEqual(s.dryGain,        0.50f)
        && nearlyEqual(s.masterGain,     1.0f)
        && (s.monitorEnabled == false)
        && nearlyEqual(s.monitorVolume,  0.8f);
}

// ── Test 3: Garbage file handling ─────────────────────────────────────────────
bool testGarbageFile()
{
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "lawliet_test_garbage";
    fs::create_directories(tmpDir);
    const fs::path iniPath = tmpDir / "garbage.ini";

    {
        std::ofstream f(iniPath);
        f << "asdfkjashdfkjahsdfkjhasdf\n"
          << "=====\n"
          << "mode=not_a_number\n"
          << "low_gain=nan\n"
          << "\0\0\1\2\3corrupted binary data\n"
          << "high_semitones=invalid_float\n"
          << "master_gain=???\n";
    }

    lawliet::config::Settings s;
    const bool res = lawliet::config::loadSettings(iniPath, s);
    fs::remove_all(tmpDir);

    // Must not crash or throw, and values should be valid/safe defaults
    return res
        && (s.mode >= 2 && s.mode <= 3)
        && (s.lowGain >= 0.0f && s.lowGain <= 1.0f)
        && (s.masterGain >= 0.0f && s.masterGain <= 2.0f);
}

// ── Test 4: Out-of-range value clamping ───────────────────────────────────────
bool testOutOfRangeValues()
{
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "lawliet_test_oor";
    fs::create_directories(tmpDir);
    const fs::path iniPath = tmpDir / "oor.ini";

    {
        std::ofstream f(iniPath);
        f << "mode=999\n"
          << "low_semitones=-999.0\n"
          << "high_semitones=999.0\n"
          << "low_gain=50.0\n"
          << "high_gain=-50.0\n"
          << "dry_gain=100.0\n"
          << "master_gain=10.0\n"
          << "monitor_volume=10.0\n";
    }

    lawliet::config::Settings s;
    lawliet::config::loadSettings(iniPath, s);
    fs::remove_all(tmpDir);

    return (s.mode == 3)
        && nearlyEqual(s.lowSemitones,  -12.0f)
        && nearlyEqual(s.highSemitones,  12.0f)
        && nearlyEqual(s.lowGain,        1.0f)
        && nearlyEqual(s.highGain,       0.0f)
        && nearlyEqual(s.dryGain,        1.0f)
        && nearlyEqual(s.masterGain,     2.0f)
        && nearlyEqual(s.monitorVolume,  1.0f);
}

// ── Test 5: Unknown keys and comments ─────────────────────────────────────────
bool testUnknownKeysAndComments()
{
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "lawliet_test_comments";
    fs::create_directories(tmpDir);
    const fs::path iniPath = tmpDir / "comments.ini";

    {
        std::ofstream f(iniPath);
        f << "# This is a comment\n"
          << "\n"
          << "; Another comment style\n"
          << "unknown_custom_key = arbitrary_value\n"
          << "mode = 3\n"
          << "   # Leading spaces comment\n"
          << "foo.bar = baz\n"
          << "low_gain = 0.60\n";
    }

    lawliet::config::Settings s;
    lawliet::config::loadSettings(iniPath, s);
    fs::remove_all(tmpDir);

    return (s.mode == 3)
        && nearlyEqual(s.lowGain, 0.60f)
        && nearlyEqual(s.highGain, 0.40f); // Default preserved
}

// ── Test 6: Device names with spaces and non-ASCII characters ──────────────────
bool testDeviceNamesWithSpacesAndNonAscii()
{
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "lawliet_test_non_ascii";
    fs::create_directories(tmpDir);
    const fs::path iniPath = tmpDir / "unicode_devices.ini";

    lawliet::config::Settings s;
    s.inputDevice   = "Focusrite Scarlett 2i2 USB (Mic 1) - 麦克风 🎤";
    s.outputDevice  = "CABLE-A Input (VB-Audio Cable A) — Dérivation & Loopback";
    s.monitorDevice = "Наушники (Realtek HD Audio) [Stereo Headphones]";

    if (!lawliet::config::saveSettings(iniPath, s)) {
        fs::remove_all(tmpDir);
        return false;
    }

    lawliet::config::Settings loaded;
    if (!lawliet::config::loadSettings(iniPath, loaded)) {
        fs::remove_all(tmpDir);
        return false;
    }

    fs::remove_all(tmpDir);

    return (loaded.inputDevice   == s.inputDevice)
        && (loaded.outputDevice  == s.outputDevice)
        && (loaded.monitorDevice == s.monitorDevice);
}

struct Registrar {
    Registrar() {
        registerTest("Settings::roundTrip",                   testRoundTrip);
        registerTest("Settings::missingFile",                 testMissingFile);
        registerTest("Settings::garbageFile",                 testGarbageFile);
        registerTest("Settings::outOfRangeValues",            testOutOfRangeValues);
        registerTest("Settings::unknownKeysAndComments",      testUnknownKeysAndComments);
        registerTest("Settings::deviceNamesSpacesAndNonAscii", testDeviceNamesWithSpacesAndNonAscii);
    }
} g_registrar;

} // namespace
