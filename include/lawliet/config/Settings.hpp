#pragma once

#include <filesystem>
#include <string>

namespace lawliet::config {

struct Settings {
    int   mode = 2;               // 2 or 3
    float lowSemitones  = -6.0f;
    float highSemitones =  5.0f;
    float lowGain = 0.75f, highGain = 0.40f, dryGain = 0.50f, masterGain = 1.0f;
    std::string inputDevice, outputDevice, monitorDevice;
    bool  monitorEnabled = false;
    float monitorVolume  = 0.8f;
};

std::filesystem::path getDefaultSettingsPath();
bool loadSettings(const std::filesystem::path& path, Settings& out);
bool saveSettings(const std::filesystem::path& path, const Settings& settings);

} // namespace lawliet::config
