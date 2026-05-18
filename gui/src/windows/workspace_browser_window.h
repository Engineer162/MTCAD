#ifndef MTCAD_GUI_WINDOWS_WORKSPACE_BROWSER_WINDOW_H
#define MTCAD_GUI_WINDOWS_WORKSPACE_BROWSER_WINDOW_H

#include <string>

#include "imgui.h"

class WorkspaceBrowserWindow {
public:
    void Render(const ImGuiIO& io);
    void SetFolderIconTexture(ImTextureID texture_id);
    void SetRootDirectory(const std::string& root_directory);

private:
    void RenderFolderRow(const char* label, bool selected) const;

    ImTextureID folder_icon_texture_ = (ImTextureID)0;
    std::string root_directory_;
    std::string current_directory_;
    bool in_local_root_ = true;
    bool collapsed_ = false;
    float expanded_width_ = 300.0f;
    float animated_width_ = 300.0f;
    bool width_initialized_ = false;
};

#endif
