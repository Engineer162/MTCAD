#include "settings_window.h"

#include "../theme_manager.h"

#include <cstring>
#include <filesystem>

SettingsWindowResult SettingsWindow::Render(bool* open, UserSettings* pending_settings) {
    SettingsWindowResult result;
    if (open == nullptr || pending_settings == nullptr) {
        return result;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 300.0f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags settings_flags = ImGuiWindowFlags_NoDocking;
    bool settings_open = *open;
    ImGui::Begin("MTCAD Settings", &settings_open, settings_flags);

    ImGui::TextUnformatted("Mouse Inputs");
    static const char* drag_button_options[] = {"Left", "Right", "Middle"};
    ImGui::Combo("Viewport Drag Button", &pending_settings->viewport_drag_button, drag_button_options, IM_ARRAYSIZE(drag_button_options));
    static const char* orbit_button_options[] = {"Left", "Right", "Middle"};
    ImGui::Combo("Viewport Orbit Button", &pending_settings->viewport_orbit_button, orbit_button_options, IM_ARRAYSIZE(orbit_button_options));
    ImGui::Separator();

    ImGui::TextUnformatted("Keyboard Inputs");
    ImGui::Checkbox("Enable Keyboard Navigation", &pending_settings->keyboard_navigation_enabled);
    ImGui::TextWrapped("Additional input preferences can be added here as more windows/tools are introduced.");
    ImGui::Separator();

    ImGui::TextUnformatted("Workspace");
    if (workspace_root_cached_ != pending_settings->workspace_root) {
        workspace_root_cached_ = pending_settings->workspace_root;
        std::snprintf(workspace_root_buffer_, sizeof(workspace_root_buffer_), "%s", workspace_root_cached_.c_str());
    }
    if (ImGui::InputText("Workspace Directory", workspace_root_buffer_, IM_ARRAYSIZE(workspace_root_buffer_))) {
        pending_settings->workspace_root = workspace_root_buffer_;
        workspace_root_cached_ = pending_settings->workspace_root;
    }
    if (ImGui::Button("Use Current Folder")) {
        pending_settings->workspace_root = std::filesystem::current_path().string();
        workspace_root_cached_ = pending_settings->workspace_root;
        std::snprintf(workspace_root_buffer_, sizeof(workspace_root_buffer_), "%s", workspace_root_cached_.c_str());
    }
    ImGui::TextWrapped("Sets the root folder shown under Local in the workspace browser.");
    ImGui::Separator();

    ImGui::TextUnformatted("Accessibility");
    render_theme_combo("Theme", &pending_settings->theme_index);
    ImGui::SliderFloat("UI Text Scale", &pending_settings->text_scale, 0.80f, 2.00f, "%.2fx");
    ImGui::SliderFloat("UI Icon Scale", &pending_settings->icon_scale, 0.80f, 2.00f, "%.2fx");
    if (ImGui::Button("Reset UI Scale")) {
        pending_settings->text_scale = 1.0f;
        pending_settings->icon_scale = 1.0f;
    }
    ImGui::TextWrapped("Text scale updates all UI text. Icon scale adjusts default icon size where icons are used.");

    ImGui::Separator();
    if (ImGui::Button("Cancel")) {
        result.cancel_pressed = true;
        settings_open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        result.apply_pressed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("OK")) {
        result.apply_pressed = true;
        settings_open = false;
    }

    ImGui::End();

    if (!settings_open && !result.apply_pressed && !result.cancel_pressed) {
        result.cancel_pressed = true;
    }

    *open = settings_open;
    return result;
}
