// src/gui/GuiController.cpp
// Lean, single-window Dear ImGui interface for Lawliet Voice Changer.
// Features: Mode II / Mode III toggle, dual-pitch sliders, gains, device routing,
// level meter, engine status, and debounced INI persistence.
// Styled in a sleek, minimalist monochrome theme with Iosevka Nerd Font.

#include "lawliet/gui/GuiController.hpp"
#include "lawliet/gui/IconPixels.hpp"
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
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace lawliet::gui {

namespace {

constexpr int IDR_FONT_IOSEVKA = 200;

struct EmbeddedFont {
    const void* data = nullptr;
    DWORD size = 0;
};

EmbeddedFont getEmbeddedFont() {
#ifdef _WIN32
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_FONT_IOSEVKA), MAKEINTRESOURCEA(10) /* RT_RCDATA */);
    if (hRes) {
        HGLOBAL hMem = LoadResource(NULL, hRes);
        if (hMem) {
            void* pData = LockResource(hMem);
            DWORD size = SizeofResource(NULL, hRes);
            if (pData && size > 100) {
                return { pData, size };
            }
        }
    }
#endif
    return { nullptr, 0 };
}

std::string findIosevkaFontPath() {
    // 1. Check relative paths (app directory, assets, fonts)
    const std::vector<std::filesystem::path> localPaths = {
        "IosevkaNerdFontPropo-SemiBold.ttf",
        "assets/fonts/IosevkaNerdFontPropo-SemiBold.ttf",
        "fonts/IosevkaNerdFontPropo-SemiBold.ttf",
        "../assets/fonts/IosevkaNerdFontPropo-SemiBold.ttf"
    };
    for (const auto& p : localPaths) {
        if (std::filesystem::exists(p)) return p.string();
    }

    // 2. Check Windows User Fonts: %LOCALAPPDATA%/Microsoft/Windows/Fonts
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && *localAppData) {
        std::filesystem::path userFonts = std::filesystem::path(localAppData) / "Microsoft" / "Windows" / "Fonts";
        if (std::filesystem::exists(userFonts)) {
            const char* preferred[] = {
                "IosevkaNerdFontPropo-SemiBold.ttf",
                "IosevkaNerdFontPropo-Regular.ttf",
                "IosevkaNerdFont-SemiBold.ttf",
                "IosevkaNerdFont-Regular.ttf",
                "IosevkaNerdFontPropo-Medium.ttf",
                "IosevkaNerdFont-Medium.ttf"
            };
            for (const char* name : preferred) {
                auto p = userFonts / name;
                if (std::filesystem::exists(p)) return p.string();
            }

            // General search for any Iosevka TTF
            for (const auto& entry : std::filesystem::directory_iterator(userFonts)) {
                if (entry.is_regular_file() && entry.path().extension() == ".ttf") {
                    std::string fn = entry.path().filename().string();
                    if (fn.find("Iosevka") != std::string::npos) {
                        return entry.path().string();
                    }
                }
            }
        }
    }

    // 3. Check Windows System Fonts: C:/Windows/Fonts
    std::filesystem::path winFonts = "C:\\Windows\\Fonts";
    if (std::filesystem::exists(winFonts)) {
        const char* preferred[] = {
            "IosevkaNerdFontPropo-SemiBold.ttf",
            "IosevkaNerdFontPropo-Regular.ttf",
            "IosevkaNerdFont-SemiBold.ttf",
            "IosevkaNerdFont-Regular.ttf"
        };
        for (const char* name : preferred) {
            auto p = winFonts / name;
            if (std::filesystem::exists(p)) return p.string();
        }

        for (const auto& entry : std::filesystem::directory_iterator(winFonts)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ttf") {
                std::string fn = entry.path().filename().string();
                if (fn.find("Iosevka") != std::string::npos) {
                    return entry.path().string();
                }
            }
        }
    }

    return "";
}

std::string findFallbackFontPath() {
    const std::vector<std::string> fallbacks = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\arial.ttf"
    };
    for (const auto& p : fallbacks) {
        if (std::filesystem::exists(p)) return p;
    }
    return "";
}

void applyMonochromeTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Sizing & Spacing (scaled up for enhanced readability, tactile controls, and zero pixelation)
    style.WindowRounding    = 8.0f;
    style.FrameRounding     = 5.0f;
    style.GrabRounding      = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding       = 5.0f;
    style.GrabMinSize       = 14.0f;
    style.ScrollbarSize     = 13.0f;
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.FramePadding      = ImVec2(9.0f, 6.0f);
    style.WindowPadding     = ImVec2(16.0f, 14.0f);

    ImVec4* colors = style.Colors;
    // Pure monochrome palette: greys, blacks, and whites
    colors[ImGuiCol_Text]                  = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.10f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.50f, 0.50f, 0.50f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.70f, 0.70f, 0.70f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.40f, 0.40f, 0.40f, 0.50f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(0.90f, 0.90f, 0.90f, 0.90f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
}

void saveFramebufferToBmp(const char* filename, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    uint32_t fileSize = 54 + width * height * 4;
    unsigned char header[54] = {
        'B', 'M',
        static_cast<unsigned char>(fileSize),
        static_cast<unsigned char>(fileSize >> 8),
        static_cast<unsigned char>(fileSize >> 16),
        static_cast<unsigned char>(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        static_cast<unsigned char>(width),
        static_cast<unsigned char>(width >> 8),
        static_cast<unsigned char>(width >> 16),
        static_cast<unsigned char>(width >> 24),
        static_cast<unsigned char>(height),
        static_cast<unsigned char>(height >> 8),
        static_cast<unsigned char>(height >> 16),
        static_cast<unsigned char>(height >> 24),
        1, 0,
        32, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    std::vector<unsigned char> bgra(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        bgra[i * 4 + 0] = pixels[i * 4 + 2]; // B
        bgra[i * 4 + 1] = pixels[i * 4 + 1]; // G
        bgra[i * 4 + 2] = pixels[i * 4 + 0]; // R
        bgra[i * 4 + 3] = pixels[i * 4 + 3]; // A
    }

    FILE* f = std::fopen(filename, "wb");
    if (f) {
        std::fwrite(header, 1, 54, f);
        std::fwrite(bgra.data(), 1, bgra.size(), f);
        std::fclose(f);
    }
}

} // anonymous namespace

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

    // OpenGL 3.3 Core Profile, non-resizable window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Fixed-size window (500 x 810 px) scaled up for comfortable spacing and larger font
    m_window = glfwCreateWindow(500, 810, "Lawliet Voice Changer", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[GuiController] Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    // Set application window icon
    GLFWimage iconImg;
    iconImg.width = kIconWidth;
    iconImg.height = kIconHeight;
    iconImg.pixels = const_cast<unsigned char*>(kIconPixels);
    glfwSetWindowIcon(m_window, 1, &iconImg);

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable VSync

    // Initialise ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Phase 6: do not write imgui.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load Iosevka Nerd Font (embedded in executable resource, or local disk, or system fallback)
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 1;
    fontConfig.PixelSnapH = true;

    static const ImWchar glyphRanges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x2000, 0x206F, // General Punctuation
        0x2190, 0x21FF, // Arrows (including ↻ U+21BB)
        0x25A0, 0x25FF, // Geometric Shapes (including ● U+25CF, ▲ U+25B2, ○ U+25CB)
        0,
    };

    const float baseFontSize = 17.5f;
    const float headerFontSize = 24.0f;

    // 1. Check embedded executable resource first (100% self-contained standalone binary)
    EmbeddedFont embedded = getEmbeddedFont();
    if (embedded.data && embedded.size > 0) {
        std::cout << "[GuiController] Loading embedded Iosevka font from PE resource (" << embedded.size << " bytes)\n";
        fontConfig.FontDataOwnedByAtlas = false;
        m_fontRegular = io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(embedded.data), (int)embedded.size, baseFontSize, &fontConfig, glyphRanges);

        ImFontConfig headerConfig = fontConfig;
        m_fontHeader = io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(embedded.data), (int)embedded.size, headerFontSize, &headerConfig, glyphRanges);
    }

    // 2. If not embedded (e.g. tests or external build without .rc), check filesystem
    if (!m_fontRegular) {
        std::string fontPath = findIosevkaFontPath();
        if (fontPath.empty()) {
            fontPath = findFallbackFontPath();
        }
        if (!fontPath.empty() && std::filesystem::exists(fontPath)) {
            std::cout << "[GuiController] Loading font from disk: " << fontPath << '\n';
            fontConfig.FontDataOwnedByAtlas = true;
            m_fontRegular = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), baseFontSize, &fontConfig, glyphRanges);

            ImFontConfig headerConfig = fontConfig;
            m_fontHeader = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), headerFontSize, &headerConfig, glyphRanges);
        }
    }

    if (!m_fontRegular) {
        std::cout << "[GuiController] Using default font.\n";
        m_fontRegular = io.Fonts->AddFontDefault();
        m_fontHeader = m_fontRegular;
    }

    // Apply clean monochrome theme and scaled metrics
    applyMonochromeTheme();

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

void GuiController::run(lawliet::audio::AudioEngine& engine, lawliet::config::Settings& settings, const std::string& screenshotPath) {
    int frameCount = 0;
    while (!glfwWindowShouldClose(m_window)) {
        if (!screenshotPath.empty()) {
            glfwPollEvents();
        } else {
            // Phase 6: Low idle CPU cost (wait up to 1/30th second for user events)
            glfwWaitEventsTimeout(1.0 / 30.0);
        }

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
        glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);

        if (!screenshotPath.empty()) {
            frameCount++;
            if (frameCount >= 3) {
                saveFramebufferToBmp(screenshotPath.c_str(), display_w, display_h);
                break;
            }
        }
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
    if (m_fontHeader) {
        ImGui::PushFont(m_fontHeader);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    ImGui::TextUnformatted("LAWLIET");
    ImGui::PopStyleColor();
    if (m_fontHeader) {
        ImGui::PopFont();
    }

    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
    ImGui::TextDisabled("Dual-Pitch Voice Changer");

    // Bypass button in top right
    const bool bypass = engine.isBypass();
    const char* bypassLabel = bypass ? "BYPASS" : "Bypass";
    const float bypassWidth = ImGui::CalcTextSize("BYPASS").x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - bypassWidth - ImGui::GetStyle().WindowPadding.x);

    if (bypass) {
        // High-contrast monochrome active state
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 1.00f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (ImGui::Button(bypassLabel, ImVec2(bypassWidth, 0.0f))) {
            engine.setBypass(false);
        }
        ImGui::PopStyleColor(4);
    } else {
        if (ImGui::Button(bypassLabel, ImVec2(bypassWidth, 0.0f))) {
            engine.setBypass(true);
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ── 1. Mode: Selectable Buttons II and III ─────────────────────────────────
    ImGui::TextUnformatted("Voice Architecture:");
    const float btnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const float modeBtnHeight = 32.0f;

    const bool isModeII = (settings.mode == 2);
    if (isModeII) {
        // Active mode: high-contrast white button with dark text
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 1.00f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
    }
    if (ImGui::Button("II", ImVec2(btnWidth, modeBtnHeight))) {
        if (settings.mode != 2) {
            settings.mode = 2;
            engine.setMode(2);
            markDirty();
        }
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();

    const bool isModeIII = (settings.mode == 3);
    if (isModeIII) {
        // Active mode: high-contrast white button with dark text
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 1.00f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
    }
    if (ImGui::Button("III", ImVec2(btnWidth, modeBtnHeight))) {
        if (settings.mode != 3) {
            settings.mode = 3;
            engine.setMode(3);
            markDirty();
        }
    }
    ImGui::PopStyleColor(4);

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
    if (ImGui::CollapsingHeader("Gains")) {
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

    if (ImGui::CollapsingHeader("Devices")) {
        ImGui::TextUnformatted("Audio Routing");
        const float refreshWidth = ImGui::CalcTextSize("↻ Refresh").x + ImGui::GetStyle().FramePadding.x * 2.0f + 10.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - refreshWidth - ImGui::GetStyle().WindowPadding.x);
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
        if (ImGui::BeginCombo("Output (for Discord)", outLabel.c_str())) {
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
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::ProgressBar(smoothedInMeter, ImVec2(-1.0f, 22.0f), inLabel);
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // ── 7. Status Line & Transport Control ─────────────────────────────────────
    const auto state = engine.getState();
    const auto statusMsg = engine.getStatusMessage();

    const float startStopBtnWidth = 85.0f;
    ImGui::Columns(2, "StatusCol", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - startStopBtnWidth - ImGui::GetStyle().WindowPadding.x * 2.0f);

    if (state == audio::State::Running) {
        ImGui::TextColored(ImVec4(0.96f, 0.96f, 0.96f, 1.0f), "● Status: %s", statusMsg.c_str());
    } else if (state == audio::State::Error) {
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "▲ Status: %s", statusMsg.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "○ Status: %s", statusMsg.c_str());
    }

    ImGui::NextColumn();

    if (state == audio::State::Running) {
        // High-contrast white button for Stop
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 1.00f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (ImGui::Button("Stop", ImVec2(-1.0f, 26.0f))) {
            engine.stop();
        }
        ImGui::PopStyleColor(4);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.40f, 0.40f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.96f, 0.96f, 0.96f, 1.0f));
        if (ImGui::Button("Start", ImVec2(-1.0f, 26.0f))) {
            engine.start();
        }
        ImGui::PopStyleColor(4);
    }
    ImGui::Columns(1);

    ImGui::Spacing();

    // ── 8. Hints & Footer ──────────────────────────────────────────────────────
    if (!engine.isCablePresent()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        ImGui::TextWrapped("VB-Audio Virtual Cable not found. Install it from vb-audio.com/Cable and restart.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Discord: set input to \"CABLE Output\"");
    ImGui::TextDisabled("Turn Krisp and echo cancellation off");
    ImGui::TextDisabled("Double voice? Uncheck \"Listen to this device\" in Windows Sound Panel");

    ImGui::End();
}

} // namespace lawliet::gui
