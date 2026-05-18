#ifndef MTCAD_GUI_WINDOWS_SETTINGS_WINDOW_H
#define MTCAD_GUI_WINDOWS_SETTINGS_WINDOW_H

#include <string>

#include "imgui.h"

struct UserSettings {
    float text_scale = 1.0f;
    float icon_scale = 1.0f;
    int theme_index = 0;
    int viewport_drag_button = 1;
    int viewport_orbit_button = 1;
    bool keyboard_navigation_enabled = true;
    std::string workspace_root;
    int window_x = 0;
    int window_y = 0;
    int window_width = 1280;
    int window_height = 800;
    bool window_fullscreen = false;
};

struct SettingsWindowResult {
    bool apply_pressed = false;
    bool cancel_pressed = false;
};

class SettingsWindow {
public:
    SettingsWindowResult Render(bool* open, UserSettings* pending_settings);

private:
    char workspace_root_buffer_[1024] = {};
    std::string workspace_root_cached_;
};

#endif
