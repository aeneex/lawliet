// src/gui/GuiController.cpp
// Lean, single-window Dear ImGui interface for Lawliet Voice Changer.
// Features: Mode II / Mode III toggle, dual-pitch sliders, gains, device routing,
// level meter, engine status, and debounced INI persistence.

#include "lawliet/gui/GuiController.hpp"
#include "lawliet/audio/AudioEngine.hpp"
#include "lawliet/config/Settings.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace lawliet::gui {

GuiController::GuiController() = default;

GuiController::~GuiController() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

bool GuiController::init() {
    if (!glfwInit()) {
        std::cerr << "[GuiController] Failed to initialise GLFW\n";
        return false;
    }

    // OpenGL 3.3 Core Profile, non-resizable compact window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Fixed-size window (430 x 590 px) to cleanly accommodate all controls
    m_window = glfwCreateWindow(430, 590, "Lawliet Voice Changer", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[GuiController] Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable VSync

    // Initialise ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Phase 6: do not write imgui.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Sleek dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 5.0f;
    style.GrabRounding      = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.FramePadding      = ImVec2(7.0f, 5.0f);
    style.WindowPadding     = ImVec2(12.0f, 10.0f);

    // Initialise backends
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) {
        std::cerr << "[GuiController] ImGui_ImplGlfw_InitForOpenGL failed\n";
        return false;
    }
    const char* glsl_version = "#version 330";
    if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
        std::cerr << "[GuiController] ImGui_ImplOpenGL3_Init failed\n";
        return false;
    }

    return true;
}

void GuiController::markDirty() {
    m_settingsDirty = true;
    m_lastChangeTime = std::chrono::steady_clock::now();
}

void GuiController::run(lawliet::audio::AudioEngine& engine, lawliet::config::Settings& settings) {
    while (!glfwWindowShouldClose(m_window)) {
        // Phase 6: Low idle CPU cost (wait up to 1/30th second for user events)
        glfwWaitEventsTimeout(1.0 / 30.0);

        // Debounced saving: write lawliet.ini ~500 ms after last setting change
        if (m_settingsDirty) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_lastChangeTime
            ).count();
            if (elapsed >= 500) {
                lawliet::config::saveSettings(lawliet::config::getDefaultSettingsPath(), settings);
                m_settingsDirty = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderMainWindow(engine, settings);

        ImGui::Render();
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    // Save settings once on exit if any changes are pending
    if (m_settingsDirty) {
        lawliet::config::saveSettings(lawliet::config::getDefaultSettingsPath(), settings);
        m_settingsDirty = false;
    }
}

void GuiController::renderMainWindow(lawliet::audio::AudioEngine& engine, lawliet::config::Settings& settings) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("LawlietMainView", nullptr, windowFlags);

    // ── Header: Title & Bypass Toggle ──────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.78f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted("LAWLIET");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
    ImGui::TextDisabled("Dual-Pitch Voice Changer");

    // Bypass button in top right
    ImGui::SameLine(ImGui::GetWindowWidth() - 75.0f);
    const bool bypass = engine.isBypass();
    if (bypass) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.45f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.55f, 0.20f, 1.0f));
        if (ImGui::SmallButton("BYPASS")) {
            engine.setBypass(false);
        }
        ImGui::PopStyleColor(2);
    } else {
        if (ImGui::SmallButton("Bypass")) {
            engine.setBypass(true);
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── 1. Mode: Selectable Buttons II and III ─────────────────────────────────
    ImGui::TextUnformatted("Voice Architecture:");
    const float btnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    const bool isModeII = (settings.mode == 2);
    if (isModeII) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.30f, 0.35f, 1.0f));
    }
    if (ImGui::Button("II", ImVec2(btnWidth, 28.0f))) {
        if (settings.mode != 2) {
            settings.mode = 2;
            engine.setMode(2);
            markDirty();
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    const bool isModeIII = (settings.mode == 3);
    if (isModeIII) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.22f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.30f, 0.35f, 1.0f));
    }
    if (ImGui::Button("III", ImVec2(btnWidth, 28.0f))) {
        if (settings.mode != 3) {
            settings.mode = 3;
            engine.setMode(3);
            markDirty();
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── 2 & 3. Pitch Sliders (Always Clamped) ──────────────────────────────────
    if (ImGui::SliderFloat("Low pitch", &settings.lowSemitones, -12.0f, 0.0f, "%.1f st", ImGuiSliderFlags_AlwaysClamp)) {
        engine.setLowPitch(settings.lowSemitones);
        markDirty();
    }

    if (ImGui::SliderFloat("High pitch", &settings.highSemitones, 0.0f, 12.0f, "%.1f st", ImGuiSliderFlags_AlwaysClamp)) {
        engine.setHighPitch(settings.highSemitones);
        markDirty();
    }

    ImGui::TextDisabled("Ctrl+click to type a value");
    ImGui::Spacing();

    // ── 4. Gains ───────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Gains", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderFloat("Low gain", &settings.lowGain, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            engine.setLowGain(settings.lowGain);
            markDirty();
        }
        if (ImGui::SliderFloat("High gain", &settings.highGain, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            engine.setHighGain(settings.highGain);
            markDirty();
        }
        if (settings.mode == 3) {
            if (ImGui::SliderFloat("Dry gain", &settings.dryGain, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                engine.setDryGain(settings.dryGain);
                markDirty();
            }
        }
        if (ImGui::SliderFloat("Master gain", &settings.masterGain, 0.0f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
            engine.setMasterGain(settings.masterGain);
            markDirty();
        }
    }

    ImGui::Spacing();

    // ── 5. Devices ─────────────────────────────────────────────────────────────
    static std::vector<audio::AudioDevice> captureDevices;
    static std::vector<audio::AudioDevice> playbackDevices;
    static bool devicesLoaded = false;

    if (!devicesLoaded) {
        captureDevices = engine.listCaptureDevices();
        playbackDevices = engine.listPlaybackDevices();
        devicesLoaded = true;
    }

    if (ImGui::CollapsingHeader("Devices", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextUnformatted("Audio Routing");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
        if (ImGui::SmallButton("↻ Refresh")) {
            devicesLoaded = false;
        }

        // Microphone combo
        const std::string micLabel = settings.inputDevice.empty()
            ? "(Default System Microphone)"
            : settings.inputDevice;
        if (ImGui::BeginCombo("Microphone", micLabel.c_str())) {
            const bool isDef = settings.inputDevice.empty();
            if (ImGui::Selectable("(Default System Microphone)", isDef)) {
                settings.inputDevice = "";
                engine.setCaptureDevice("");
                markDirty();
            }
            if (isDef) ImGui::SetItemDefaultFocus();

            for (const auto& dev : captureDevices) {
                const bool isSel = (settings.inputDevice == dev.name);
                if (ImGui::Selectable(dev.name.c_str(), isSel)) {
                    settings.inputDevice = dev.name;
                    engine.setCaptureDevice(dev.name);
                    markDirty();
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Output combo (streams into Virtual Cable for Discord)
        const std::string outLabel = settings.outputDevice.empty()
            ? (engine.isCablePresent() ? "CABLE Input (VB-Audio Virtual Cable)" : "(Default Playback Device)")
            : settings.outputDevice;
        if (ImGui::BeginCombo("Output", outLabel.c_str())) {
            const bool isDef = settings.outputDevice.empty();
            if (ImGui::Selectable("(Default / CABLE Input)", isDef)) {
                settings.outputDevice = "";
                engine.setOutputDevice("");
                markDirty();
            }
            if (isDef) ImGui::SetItemDefaultFocus();

            for (const auto& dev : playbackDevices) {
                const bool isSel = (settings.outputDevice == dev.name);
                if (ImGui::Selectable(dev.name.c_str(), isSel)) {
                    settings.outputDevice = dev.name;
                    engine.setOutputDevice(dev.name);
                    markDirty();
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Monitor checkbox + device combo + volume
        if (ImGui::Checkbox("Hear in Headphones (Monitor)", &settings.monitorEnabled)) {
            engine.setMonitorEnabled(settings.monitorEnabled);
            markDirty();
        }

        if (settings.monitorEnabled) {
            const std::string monLabel = settings.monitorDevice.empty()
                ? "(Default Playback Device)"
                : settings.monitorDevice;
            if (ImGui::BeginCombo("Monitor Device", monLabel.c_str())) {
                const bool isDef = settings.monitorDevice.empty();
                if (ImGui::Selectable("(Default Playback Device)", isDef)) {
                    settings.monitorDevice = "";
                    engine.setMonitorDevice("");
                    markDirty();
                }
                if (isDef) ImGui::SetItemDefaultFocus();

                for (const auto& dev : playbackDevices) {
                    const bool isSel = (settings.monitorDevice == dev.name);
                    if (ImGui::Selectable(dev.name.c_str(), isSel)) {
                        settings.monitorDevice = dev.name;
                        engine.setMonitorDevice(dev.name);
                        markDirty();
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::SliderFloat("Monitor Volume", &settings.monitorVolume, 0.0f, 1.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
                engine.setMonitorVolume(settings.monitorVolume);
                markDirty();
            }
        }
    }

    ImGui::Spacing();

    // ── 6. Input Level Meter ───────────────────────────────────────────────────
    static float smoothedInMeter = 0.0f;
    const float rawIn = engine.getInputLevel();
    smoothedInMeter = std::max(rawIn, smoothedInMeter * 0.88f);
    char inLabel[32];
    std::snprintf(inLabel, sizeof(inLabel), "%.0f%%", std::clamp(smoothedInMeter * 100.0f, 0.0f, 100.0f));

    ImGui::TextUnformatted("Mic Level:");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.75f, 0.95f, 1.0f));
    ImGui::ProgressBar(smoothedInMeter, ImVec2(-1.0f, 13.0f), inLabel);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── 7. Status Line & Transport Control ─────────────────────────────────────
    const auto state = engine.getState();
    const auto statusMsg = engine.getStatusMessage();

    ImGui::Columns(2, "StatusCol", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - 95.0f);

    if (state == audio::State::Running) {
        ImGui::TextColored(ImVec4(0.20f, 0.95f, 0.35f, 1.0f), "● Status: %s", statusMsg.c_str());
    } else if (state == audio::State::Error) {
        ImGui::TextColored(ImVec4(0.95f, 0.30f, 0.30f, 1.0f), "▲ Status: %s", statusMsg.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.60f, 0.65f, 0.70f, 1.0f), "○ Status: %s", statusMsg.c_str());
    }

    ImGui::NextColumn();

    if (state == audio::State::Running) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.30f, 0.30f, 1.0f));
        if (ImGui::Button("Stop", ImVec2(-1.0f, 22.0f))) {
            engine.stop();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.58f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.70f, 0.40f, 1.0f));
        if (ImGui::Button("Start", ImVec2(-1.0f, 22.0f))) {
            engine.start();
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::Columns(1);

    ImGui::Spacing();

    // ── 8. Hints & Footer ──────────────────────────────────────────────────────
    if (!engine.isCablePresent()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
        ImGui::TextWrapped("VB-Audio Virtual Cable not found. Install it from vb-audio.com/Cable and restart.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Discord: set input to \"CABLE Output\"");
    ImGui::TextDisabled("Turn Krisp and echo cancellation off");

    ImGui::End();
}

} // namespace lawliet::gui
