// src/main.cpp
// Application entry point for Project Lawliet.

#include "lawliet/audio/AudioEngine.hpp"
#include "lawliet/config/Settings.hpp"
#include "lawliet/gui/GuiController.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "Project Lawliet v0.1.0 — real-time dual-pitch voice changer\n";

    // ── Resolve settings path ────────────────────────────────────────────────
    std::filesystem::path settingsPath = lawliet::config::getDefaultSettingsPath();
    if (argc >= 2) {
        settingsPath = argv[1];
    }

    // ── Load configuration ───────────────────────────────────────────────────
    lawliet::config::Settings settings;
    if (lawliet::config::loadSettings(settingsPath, settings)) {
        std::cout << "[main] Loaded settings from: " << settingsPath << '\n';
    } else {
        std::cout << "[main] Settings file not found — using defaults.\n";
    }

    // ── Run application ──────────────────────────────────────────────────────
    lawliet::audio::AudioEngine engine;
    if (engine.init(settings) != lawliet::audio::AudioEngineError::None) {
        std::cerr << "[main] AudioEngine init failed.\n";
        return 1;
    }
    lawliet::gui::GuiController gui;
    if (!gui.init()) {
        std::cerr << "[main] GUI init failed.\n";
        return 1;
    }
    engine.start(); // Auto-start streaming on launch just like Clownfish
    gui.run(engine, settings);

    return 0;
}
