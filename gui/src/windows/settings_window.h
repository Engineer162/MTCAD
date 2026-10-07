#ifndef MTCAD_GUI_WINDOWS_SETTINGS_WINDOW_H
#define MTCAD_GUI_WINDOWS_SETTINGS_WINDOW_H

#include "../managers/shortcut_manager.h"

#include "imgui.h"
#include "imgui_toggle.h"

#include <string>

struct UserSettings {
    float text_scale = 1.0f;
    bool use_icons_in_navbar = true;
    float icon_scale = 1.0f;
    int theme_index = 0;
    int viewport_pan_button = 0;
    int viewport_orbit_button = 1;
    bool keyboard_navigation_enabled = true;
    std::string workspace_root;
    ShortcutChord shortcut_new;
    ShortcutChord shortcut_open;
    ShortcutChord shortcut_save;
    ShortcutChord shortcut_undo;
    ShortcutChord shortcut_redo;
    ShortcutChord shortcut_settings;
    ShortcutChord shortcut_about;
    ShortcutChord shortcut_create_sketch;
    ShortcutChord shortcut_finish_sketch;
    ShortcutChord shortcut_extrude;
    ShortcutChord shortcut_revolve;
    ShortcutChord shortcut_line;
    ShortcutChord shortcut_rectangle;
    ShortcutChord shortcut_circle;
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
    bool IsCapturingShortcut() const;

private:
    enum class Section {
        General = 0,
        Workspace,
        Viewport,
        Accessibility,
        Shortcuts,
    };

    Section selected_section_ = Section::General;
    char workspace_root_buffer_[1024] = {};
    std::string workspace_root_cached_;
    ShortcutAssignmentState shortcut_new_state_;
    ShortcutAssignmentState shortcut_open_state_;
    ShortcutAssignmentState shortcut_save_state_;
    ShortcutAssignmentState shortcut_undo_state_;
    ShortcutAssignmentState shortcut_redo_state_;
    ShortcutAssignmentState shortcut_settings_state_;
    ShortcutAssignmentState shortcut_about_state_;
    ShortcutAssignmentState shortcut_create_sketch_state_;
    ShortcutAssignmentState shortcut_finish_sketch_state_;
    ShortcutAssignmentState shortcut_extrude_state_;
    ShortcutAssignmentState shortcut_revolve_state_;
    ShortcutAssignmentState shortcut_line_state_;
    ShortcutAssignmentState shortcut_rectangle_state_;
    ShortcutAssignmentState shortcut_circle_state_;
};

#endif
