// src/config/Settings.cpp
// Plain INI settings loader and atomic saver for Lawliet.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "lawliet/config/Settings.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

namespace lawliet::config {

static std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static float parseClampedFloat(const std::string& str, float minVal, float maxVal, float fallback) {
    char* end = nullptr;
    const float v = std::strtof(str.c_str(), &end);
    if (end == str.c_str() || std::isnan(v) || std::isinf(v)) return fallback;
    return std::clamp(v, minVal, maxVal);
}

static int parseClampedInt(const std::string& str, int minVal, int maxVal, int fallback) {
    char* end = nullptr;
    const long v = std::strtol(str.c_str(), &end, 10);
    if (end == str.c_str()) return fallback;
    return std::clamp(static_cast<int>(v), minVal, maxVal);
}

static bool parseBool(const std::string& str, bool fallback) {
    if (str == "1" || str == "true" || str == "yes" || str == "on") return true;
    if (str == "0" || str == "false" || str == "no" || str == "off") return false;
    return fallback;
}

std::filesystem::path getDefaultSettingsPath() {
    wchar_t buf[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(buf).parent_path() / "lawliet.ini";
    }
    return "lawliet.ini";
}

bool loadSettings(const std::filesystem::path& path, Settings& out) {
    Settings s; // defaults
    std::ifstream f(path);
    if (!f.is_open()) {
        out = s;
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        const auto t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        const auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(t.substr(0, eq));
        const std::string val = trim(t.substr(eq + 1));
        if (key == "mode")                  s.mode = parseClampedInt(val, 2, 3, s.mode);
        else if (key == "low_semitones")   s.lowSemitones = parseClampedFloat(val, -12.0f, 0.0f, s.lowSemitones);
        else if (key == "high_semitones")  s.highSemitones = parseClampedFloat(val, 0.0f, 12.0f, s.highSemitones);
        else if (key == "low_gain")        s.lowGain = parseClampedFloat(val, 0.0f, 1.0f, s.lowGain);
        else if (key == "high_gain")       s.highGain = parseClampedFloat(val, 0.0f, 1.0f, s.highGain);
        else if (key == "dry_gain")        s.dryGain = parseClampedFloat(val, 0.0f, 1.0f, s.dryGain);
        else if (key == "master_gain")     s.masterGain = parseClampedFloat(val, 0.0f, 2.0f, s.masterGain);
        else if (key == "input_device")    s.inputDevice = val;
        else if (key == "output_device")   s.outputDevice = val;
        else if (key == "monitor_device")  s.monitorDevice = val;
        else if (key == "monitor_enabled") s.monitorEnabled = parseBool(val, s.monitorEnabled);
        else if (key == "monitor_volume")  s.monitorVolume = parseClampedFloat(val, 0.0f, 1.0f, s.monitorVolume);
    }
    out = s;
    return true;
}

bool saveSettings(const std::filesystem::path& path, const Settings& s) {
    auto tmpPath = path;
    tmpPath += ".tmp";
    std::ofstream f(tmpPath, std::ios::trunc);
    if (!f.is_open()) return false;

    f << "mode="            << std::clamp(s.mode, 2, 3) << "\n"
      << "low_semitones="   << std::clamp(s.lowSemitones, -12.0f, 0.0f) << "\n"
      << "high_semitones="  << std::clamp(s.highSemitones, 0.0f, 12.0f) << "\n"
      << "low_gain="        << std::clamp(s.lowGain, 0.0f, 1.0f) << "\n"
      << "high_gain="       << std::clamp(s.highGain, 0.0f, 1.0f) << "\n"
      << "dry_gain="        << std::clamp(s.dryGain, 0.0f, 1.0f) << "\n"
      << "master_gain="     << std::clamp(s.masterGain, 0.0f, 2.0f) << "\n"
      << "input_device="    << s.inputDevice << "\n"
      << "output_device="   << s.outputDevice << "\n"
      << "monitor_enabled=" << (s.monitorEnabled ? "true" : "false") << "\n"
      << "monitor_device="  << s.monitorDevice << "\n"
      << "monitor_volume="  << std::clamp(s.monitorVolume, 0.0f, 1.0f) << "\n";
    f.close();

    const std::wstring wtmp = tmpPath.wstring();
    const std::wstring wdst = path.wstring();
    return MoveFileExW(wtmp.c_str(), wdst.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

} // namespace lawliet::config
