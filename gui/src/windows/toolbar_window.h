#ifndef MTCAD_GUI_WINDOWS_TOOLBAR_WINDOW_H
#define MTCAD_GUI_WINDOWS_TOOLBAR_WINDOW_H

#include "imgui.h"

class ToolbarWindow {
public:
    enum class IconId {
        Extrude = 0,
        Revolve,
        Hole,
        PressPull,
        Chamfer,
        Fillet,
        Shell,
        Combine,
        SplitBody,
        Plane,
        Axes,
        Measure,
        Section,
        Interference,
        Point,
        Mirror,
        Line,
        Rectangle,
        Circle,
        Arc,
        Text,
    };

    void SetIconTexture(IconId icon_id, ImTextureID texture_id);

    void SetIconScale(float icon_scale);
    void SetSketchMode(bool enabled);
    bool ConsumeBeginSketchRequest();
    bool ConsumeBeginSolidModeRequest();
    bool ConsumeSelectedTool(const char** out_tool_name);
    void Render(const ImGuiIO& io);

private:
    bool sketch_mode_ = false;
    bool begin_sketch_request_pending_ = false;
    bool begin_solid_mode_request_pending_ = true;
    const char* selected_tool_name_pending_ = nullptr;
    const char* active_tool_name_ = nullptr;

    // Solid tools icons
    ImTextureID extrude_icon_texture_ = (ImTextureID)0;
    ImTextureID revolve_icon_texture_ = (ImTextureID)0;
    ImTextureID hole_icon_texture_ = (ImTextureID)0;
    ImTextureID press_pull_icon_texture_ = (ImTextureID)0;
    ImTextureID chamfer_icon_texture_ = (ImTextureID)0;
    ImTextureID fillet_icon_texture_ = (ImTextureID)0;
    ImTextureID shell_icon_texture_ = (ImTextureID)0;
    ImTextureID combine_icon_texture_ = (ImTextureID)0;
    ImTextureID split_body_icon_texture_ = (ImTextureID)0;
    ImTextureID plane_icon_texture_ = (ImTextureID)0;
    ImTextureID axes_icon_texture_ = (ImTextureID)0;
    ImTextureID measure_icon_texture_ = (ImTextureID)0;
    ImTextureID section_icon_texture_ = (ImTextureID)0;
    ImTextureID interference_icon_texture_ = (ImTextureID)0;

    // Shared icons
    ImTextureID point_icon_texture_ = (ImTextureID)0;
    ImTextureID mirror_icon_texture_ = (ImTextureID)0;
    // Sketch tools icons
    ImTextureID line_icon_texture_ = (ImTextureID)0;
    ImTextureID rectangle_icon_texture_ = (ImTextureID)0;
    ImTextureID circle_icon_texture_ = (ImTextureID)0;
    ImTextureID arc_icon_texture_ = (ImTextureID)0;
    ImTextureID text_icon_texture_ = (ImTextureID)0;

    float icon_scale_ = 1.0f;
    bool snap_to_grid_ = true;
    bool ortho_mode_ = false;
    float grid_step_ = 1.0f;
};

#endif
