#include "workspace_browser_window.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <vector>

#include "imgui_internal.h"

namespace {
constexpr float kCollapsedWidth = 42.0f;
constexpr float kMinExpandedWidth = 240.0f;
}

void WorkspaceBrowserWindow::SetFolderIconTexture(ImTextureID texture_id) {
    folder_icon_texture_ = texture_id;
}

void WorkspaceBrowserWindow::SetRootDirectory(const std::string& root_directory) {
    root_directory_ = root_directory;
    if (root_directory_.empty()) {
        root_directory_ = std::filesystem::current_path().string();
    }

    std::error_code ec;
    if (!std::filesystem::exists(root_directory_, ec) || !std::filesystem::is_directory(root_directory_, ec)) {
        root_directory_ = std::filesystem::current_path().string();
    }

    in_local_root_ = true;
    current_directory_.clear();
}

void WorkspaceBrowserWindow::RenderFolderRow(const char* label, bool selected) const {
    if (folder_icon_texture_ != (ImTextureID)0) {
        const float icon_size = ImGui::GetTextLineHeight();
        ImGui::Image(folder_icon_texture_, ImVec2(icon_size, icon_size));
        ImGui::SameLine();
    }

    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 220, 255, 255));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
    } else {
        ImGui::TextUnformatted(label);
    }
}

void WorkspaceBrowserWindow::Render(const ImGuiIO& io) {
    if (!width_initialized_) {
        animated_width_ = expanded_width_;
        width_initialized_ = true;
    }

    const float target_width = collapsed_ ? kCollapsedWidth : expanded_width_;
    const bool is_animating = std::fabs(target_width - animated_width_) > 0.5f;
    const float animation_speed = 1200.0f;
    const float step = animation_speed * ((io.DeltaTime > 0.0f) ? io.DeltaTime : (1.0f / 60.0f));
    const float delta = target_width - animated_width_;
    if (delta > step) {
        animated_width_ += step;
    } else if (delta < -step) {
        animated_width_ -= step;
    } else {
        animated_width_ = target_width;
    }

    ImGui::SetNextWindowSize(ImVec2(expanded_width_, 420.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Workspace", nullptr, ImGuiWindowFlags_NoScrollbar);

    const bool is_docked = ImGui::IsWindowDocked();
    ImGuiWindow* current_window = ImGui::GetCurrentWindow();

    if (is_animating) {
        if (is_docked && current_window->DockNode != nullptr) {
            current_window->DockNode->SizeRef.x = animated_width_;
        } else {
            ImGui::SetWindowSize(ImVec2(animated_width_, ImGui::GetWindowHeight()));
        }
    }

    const float content_min_x = ImGui::GetWindowContentRegionMin().x;
    const float content_max_x = ImGui::GetWindowContentRegionMax().x;
    const char* collapse_label = collapsed_ ? ">" : "<";
    const float collapse_button_width = 26.0f;
    ImGui::SetCursorPosX(content_max_x - collapse_button_width);
    if (ImGui::Button(collapse_label, ImVec2(collapse_button_width, 0.0f))) {
        if (!collapsed_) {
            if (is_docked && current_window->DockNode != nullptr) {
                expanded_width_ = (std::max)(kMinExpandedWidth, current_window->DockNode->Size.x);
            } else {
                expanded_width_ = (std::max)(kMinExpandedWidth, ImGui::GetWindowWidth());
            }
        }
        collapsed_ = !collapsed_;
    }

    const float denom = (expanded_width_ - kCollapsedWidth);
    float t = (denom > 1e-3f) ? ((animated_width_ - kCollapsedWidth) / denom) : 1.0f;
    if (t < 0.0f) {
        t = 0.0f;
    }
    if (t > 1.0f) {
        t = 1.0f;
    }

    const float style_spacing = ImGui::GetStyle().ItemSpacing.x;
    const float full_content_width = (content_max_x - content_min_x) - collapse_button_width - style_spacing;
    const float visible_content_width = full_content_width * t;

    if (visible_content_width <= 2.0f) {
        ImGui::End();
        return;
    }

    ImGui::SetCursorPosX(content_min_x);
    ImGui::BeginChild("workspace_content", ImVec2(visible_content_width, 0.0f), false, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextUnformatted("Workspace Browser");
    ImGui::Separator();

    if (in_local_root_) {
        ImGui::PushID("local_root");
        if (ImGui::Selectable("##local_root_select", false)) {
            in_local_root_ = false;
            current_directory_ = root_directory_;
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing());
        RenderFolderRow("Local", false);
        ImGui::PopID();

        ImGui::Spacing();
        ImGui::TextWrapped("Root: %s", root_directory_.c_str());
    } else {
        if (ImGui::Button("Back")) {
            in_local_root_ = true;
            current_directory_.clear();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Local");
        ImGui::TextWrapped("%s", current_directory_.c_str());
        ImGui::Separator();

        std::error_code ec;
        std::vector<std::filesystem::directory_entry> entries;
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(current_directory_, ec)) {
            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(), [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
            const bool a_dir = a.is_directory();
            const bool b_dir = b.is_directory();
            if (a_dir != b_dir) {
                return a_dir > b_dir;
            }
            return a.path().filename().string() < b.path().filename().string();
        });

        for (const std::filesystem::directory_entry& entry : entries) {
            const bool is_directory = entry.is_directory();
            const std::string entry_name = entry.path().filename().string();

            ImGui::PushID(entry_name.c_str());
            if (ImGui::Selectable("##entry_select", false)) {
                if (is_directory) {
                    current_directory_ = entry.path().string();
                }
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing());
            if (is_directory) {
                RenderFolderRow(entry_name.c_str(), false);
            } else {
                ImGui::TextUnformatted(entry_name.c_str());
            }
            ImGui::PopID();
        }

        if (entries.empty()) {
            ImGui::TextDisabled("No files or folders.");
        }
    }

    ImGui::EndChild();

    ImGui::End();
}
