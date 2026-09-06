#include "settings_io.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <SDL3/SDL.h>

#include "../managers/path_manager.h"
#include "../managers/shortcut_manager.h"
#include "../managers/theme_manager.h"

namespace {

float clampf(float v, float min_v, float max_v) {
    if (v < min_v) {
        return min_v;
    }
    if (v > max_v) {
        return max_v;
    }
    return v;
}

std::string trim_copy(const std::string& value) {
    const std::string ws = " \t\r\n";
    const size_t start = value.find_first_not_of(ws);
    if (start == std::string::npos) {
        return std::string();
    }
    const size_t end = value.find_last_not_of(ws);
    return value.substr(start, end - start + 1);
}

} // namespace

namespace mtcad {
namespace runtime {

void ApplyDefaultShortcuts(UserSettings* settings) {
    if (settings == nullptr) {
        return;
    }

    settings->shortcut_new.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_N};
    settings->shortcut_open.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_O};
    settings->shortcut_save.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_S};
    settings->shortcut_undo.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_Z};
    settings->shortcut_redo.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_Y};
    settings->shortcut_settings.scancodes = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_COMMA};
    settings->shortcut_about.scancodes = {SDL_SCANCODE_F1};
    settings->shortcut_create_sketch.scancodes = {SDL_SCANCODE_K};
    settings->shortcut_finish_sketch.scancodes = {SDL_SCANCODE_F};
    settings->shortcut_extrude.scancodes = {SDL_SCANCODE_E};
    settings->shortcut_revolve.scancodes = {SDL_SCANCODE_R};
    settings->shortcut_line.scancodes = {SDL_SCANCODE_L};
    settings->shortcut_rectangle.scancodes = {SDL_SCANCODE_Q};
    settings->shortcut_circle.scancodes = {SDL_SCANCODE_C};
}

void EnsureDefaultImGuiIni(const std::filesystem::path& settings_dir)
{
    const std::filesystem::path imgui_ini_path = settings_dir / "imgui.ini";
    if (std::filesystem::exists(imgui_ini_path)) {
        return;
    }

    const std::filesystem::path default_imgui_ini = get_app_lib_directory() / "imgui.ini";
    if (!std::filesystem::exists(default_imgui_ini)) {
        return;
    }

    std::error_code ec;
    std::filesystem::copy_file(default_imgui_ini, imgui_ini_path, std::filesystem::copy_options::overwrite_existing, ec);
}

bool LoadUserSettingsIni(const char* file_path, UserSettings* out_settings) {
    if (file_path == nullptr || out_settings == nullptr) {
        return false;
    }

    std::ifstream in(file_path);
    if (!in.is_open()) {
        return false;
    }

    UserSettings loaded = *out_settings;
    std::string line;
    while (std::getline(in, line)) {
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(line.substr(0, equals));
        const std::string value = trim_copy(line.substr(equals + 1));
        if (key.empty()) {
            continue;
        }

        try {
            if (key == "text_scale") {
                loaded.text_scale = std::stof(value);
            } else if (key == "icon_scale") {
                loaded.icon_scale = std::stof(value);
            } else if (key == "theme_index") {
                loaded.theme_index = std::stoi(value);
            } else if (key == "viewport_pan_button") {
                loaded.viewport_pan_button = std::stoi(value);
            } else if (key == "viewport_orbit_button") {
                loaded.viewport_orbit_button = std::stoi(value);
            } else if (key == "keyboard_navigation") {
                loaded.keyboard_navigation_enabled = (std::stoi(value) != 0);
            } else if (key == "workspace_root") {
                loaded.workspace_root = value;
            } else if (key == "window_x") {
                loaded.window_x = std::stoi(value);
            } else if (key == "window_y") {
                loaded.window_y = std::stoi(value);
            } else if (key == "window_width") {
                loaded.window_width = std::stoi(value);
            } else if (key == "window_height") {
                loaded.window_height = std::stoi(value);
            } else if (key == "window_fullscreen") {
                loaded.window_fullscreen = (std::stoi(value) != 0);
            } else if (key == "shortcut_new") {
                ShortcutFromStorageString(value, &loaded.shortcut_new);
            } else if (key == "shortcut_open") {
                ShortcutFromStorageString(value, &loaded.shortcut_open);
            } else if (key == "shortcut_save") {
                ShortcutFromStorageString(value, &loaded.shortcut_save);
            } else if (key == "shortcut_undo") {
                ShortcutFromStorageString(value, &loaded.shortcut_undo);
            } else if (key == "shortcut_redo") {
                ShortcutFromStorageString(value, &loaded.shortcut_redo);
            } else if (key == "shortcut_settings") {
                ShortcutFromStorageString(value, &loaded.shortcut_settings);
            } else if (key == "shortcut_about") {
                ShortcutFromStorageString(value, &loaded.shortcut_about);
            } else if (key == "shortcut_create_sketch") {
                ShortcutFromStorageString(value, &loaded.shortcut_create_sketch);
            } else if (key == "shortcut_finish_sketch") {
                ShortcutFromStorageString(value, &loaded.shortcut_finish_sketch);
            } else if (key == "shortcut_extrude") {
                ShortcutFromStorageString(value, &loaded.shortcut_extrude);
            } else if (key == "shortcut_revolve") {
                ShortcutFromStorageString(value, &loaded.shortcut_revolve);
            } else if (key == "shortcut_line") {
                ShortcutFromStorageString(value, &loaded.shortcut_line);
            } else if (key == "shortcut_rectangle") {
                ShortcutFromStorageString(value, &loaded.shortcut_rectangle);
            } else if (key == "shortcut_circle") {
                ShortcutFromStorageString(value, &loaded.shortcut_circle);
            }
        } catch (...) {
            continue;
        }
    }

    loaded.text_scale = clampf(loaded.text_scale, 0.80f, 2.00f);
    loaded.icon_scale = clampf(loaded.icon_scale, 0.80f, 2.00f);
    loaded.theme_index = clamp_theme_index(loaded.theme_index);

    if (loaded.viewport_pan_button < 0 || loaded.viewport_pan_button > 2) {
        loaded.viewport_pan_button = 0;
    }
    if (loaded.viewport_orbit_button < 0 || loaded.viewport_orbit_button > 2) {
        loaded.viewport_orbit_button = 1;
    }
    if (loaded.window_width < 640) {
        loaded.window_width = 640;
    }
    if (loaded.window_height < 480) {
        loaded.window_height = 480;
    }
    if (loaded.workspace_root.empty()) {
        loaded.workspace_root = std::filesystem::current_path().string();
    }

    *out_settings = loaded;
    return true;
}

bool SaveUserSettingsIni(const char* file_path, const UserSettings& settings) {
    if (file_path == nullptr) {
        return false;
    }

    std::ofstream out(file_path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << "[MTCAD]\n";
    out << "text_scale=" << settings.text_scale << "\n";
    out << "icon_scale=" << settings.icon_scale << "\n";
    out << "theme_index=" << clamp_theme_index(settings.theme_index) << "\n";
    out << "viewport_pan_button=" << settings.viewport_pan_button << "\n";
    out << "viewport_orbit_button=" << settings.viewport_orbit_button << "\n";
    out << "keyboard_navigation=" << (settings.keyboard_navigation_enabled ? 1 : 0) << "\n";
    out << "workspace_root=" << settings.workspace_root << "\n";
    out << "window_x=" << settings.window_x << "\n";
    out << "window_y=" << settings.window_y << "\n";
    out << "window_width=" << settings.window_width << "\n";
    out << "window_height=" << settings.window_height << "\n";
    out << "window_fullscreen=" << (settings.window_fullscreen ? 1 : 0) << "\n";
    out << "shortcut_new=" << ShortcutToStorageString(settings.shortcut_new) << "\n";
    out << "shortcut_open=" << ShortcutToStorageString(settings.shortcut_open) << "\n";
    out << "shortcut_save=" << ShortcutToStorageString(settings.shortcut_save) << "\n";
    out << "shortcut_undo=" << ShortcutToStorageString(settings.shortcut_undo) << "\n";
    out << "shortcut_redo=" << ShortcutToStorageString(settings.shortcut_redo) << "\n";
    out << "shortcut_settings=" << ShortcutToStorageString(settings.shortcut_settings) << "\n";
    out << "shortcut_about=" << ShortcutToStorageString(settings.shortcut_about) << "\n";
    out << "shortcut_create_sketch=" << ShortcutToStorageString(settings.shortcut_create_sketch) << "\n";
    out << "shortcut_finish_sketch=" << ShortcutToStorageString(settings.shortcut_finish_sketch) << "\n";
    out << "shortcut_extrude=" << ShortcutToStorageString(settings.shortcut_extrude) << "\n";
    out << "shortcut_revolve=" << ShortcutToStorageString(settings.shortcut_revolve) << "\n";
    out << "shortcut_line=" << ShortcutToStorageString(settings.shortcut_line) << "\n";
    out << "shortcut_rectangle=" << ShortcutToStorageString(settings.shortcut_rectangle) << "\n";
    out << "shortcut_circle=" << ShortcutToStorageString(settings.shortcut_circle) << "\n";
    return out.good();
}

} // namespace runtime
} // namespace mtcad
