#pragma once

#include <chrono>
#include <string>

struct GLFWwindow;
struct ImFont;

namespace lawliet::audio { class AudioEngine; }
namespace lawliet::config { struct Settings; }

namespace lawliet::gui {

class GuiController {
public:
    GuiController();
    ~GuiController();

    bool init();
    void run(lawliet::audio::AudioEngine& engine, lawliet::config::Settings& settings, const std::string& screenshotPath = "");

private:
    void renderMainWindow(lawliet::audio::AudioEngine& engine, lawliet::config::Settings& settings);
    void markDirty();

    GLFWwindow* m_window = nullptr;
    bool m_settingsDirty = false;
    std::chrono::steady_clock::time_point m_lastChangeTime{};

    ImFont* m_fontRegular = nullptr;
    ImFont* m_fontHeader = nullptr;
};

} // namespace lawliet::gui
