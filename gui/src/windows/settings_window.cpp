#include "settings_window.h"

#include "../managers/theme_manager.h"

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

    const float action_row_height = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    const float nav_width = 170.0f;
    ImGui::BeginChild("settings_main_split", ImVec2(0.0f, -action_row_height), false);

    ImGui::BeginChild("settings_sections", ImVec2(nav_width, 0.0f), true);
    ImGui::TextUnformatted("Settings");
    ImGui::Separator();
    if (ImGui::Selectable("General", selected_section_ == Section::General)) {
        selected_section_ = Section::General;
    }
    if (ImGui::Selectable("Workspace", selected_section_ == Section::Workspace)) {
        selected_section_ = Section::Workspace;
    }
    if (ImGui::Selectable("Viewport", selected_section_ == Section::Viewport)) {
        selected_section_ = Section::Viewport;
    }
    if (ImGui::Selectable("Accessibility", selected_section_ == Section::Accessibility)) {
        selected_section_ = Section::Accessibility;
    }
    if (ImGui::Selectable("Shortcuts", selected_section_ == Section::Shortcuts)) {
        selected_section_ = Section::Shortcuts;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("settings_details", ImVec2(0.0f, 0.0f), true);
    if (selected_section_ == Section::General) {
        ImGui::TextUnformatted("General");
        ImGui::Separator();
        ImGui::Checkbox("Enable Keyboard Navigation", &pending_settings->keyboard_navigation_enabled);
        ImGui::TextWrapped("Additional input preferences can be added here as more windows/tools are introduced.");
    } else if (selected_section_ == Section::Workspace) {
        ImGui::TextUnformatted("Workspace");
        ImGui::Separator();
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
    } else if (selected_section_ == Section::Viewport) {
        ImGui::TextUnformatted("Viewport");
        ImGui::Separator();
        static const char* pan_button_options[] = {"Left", "Right", "Middle"};
        ImGui::Combo("Viewport Pan Button", &pending_settings->viewport_pan_button, pan_button_options, IM_ARRAYSIZE(pan_button_options));
        static const char* orbit_button_options[] = {"Left", "Right", "Middle"};
        ImGui::Combo("Viewport Orbit Button", &pending_settings->viewport_orbit_button, orbit_button_options, IM_ARRAYSIZE(orbit_button_options));
    } else if (selected_section_ == Section::Accessibility) {
        ImGui::TextUnformatted("Accessibility");
        ImGui::Separator();
        render_theme_combo("Theme", &pending_settings->theme_index);
        ImGui::SliderFloat("UI Text Scale", &pending_settings->text_scale, 0.80f, 2.00f, "%.2fx");
        ImGui::SliderFloat("UI Icon Scale", &pending_settings->icon_scale, 0.80f, 2.00f, "%.2fx");
        if (ImGui::Button("Reset UI Scale")) {
            pending_settings->text_scale = 1.0f;
            pending_settings->icon_scale = 1.0f;
        }
        ImGui::TextWrapped("Text scale updates all UI text. Icon scale adjusts default icon size where icons are used.");
    } else if (selected_section_ == Section::Shortcuts) {
        ImGui::TextUnformatted("Shortcuts");
        ImGui::Separator();
        ImGui::TextWrapped("Shortcut customization is not yet implemented. This section is ready for command mapping controls.");
    }
    ImGui::EndChild();

    ImGui::EndChild();

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
