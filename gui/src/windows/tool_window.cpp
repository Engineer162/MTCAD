#include "tool_window.h"

namespace mtcad {

void SketchPaletteWindow::Open() {
    open_ = true;
}

void SketchPaletteWindow::Close() {
    open_ = false;
}

bool SketchPaletteWindow::ConsumeFinishSketch() {
    if (!finish_sketch_requested_) return false;
    finish_sketch_requested_ = false;
    return true;
}

bool SketchPaletteWindow::ConsumeCancelSketch() {
    if (!cancel_sketch_requested_) return false;
    cancel_sketch_requested_ = false;
    return true;
}

void SketchPaletteWindow::Render(const ImVec2& viewport_pos, const ImVec2& viewport_size) {
    if (!open_) return;

    // Initial position at right-middle of viewport (user can move/dock freely)
    ImVec2 pos = ImVec2(viewport_pos.x + viewport_size.x - size_.x - padding_,
                        viewport_pos.y + viewport_size.y * 0.5f - size_.y * 0.5f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size_, ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = 0;

    if (ImGui::Begin("Sketch Palette", nullptr, flags)) {
        // Content area
        ImGui::BeginChild("sketch_palette_content", ImVec2(0, -50), false);

        ImGui::TextUnformatted("Line Type");

        bool construction_mode_checked = settings_.construction_mode;
        bool centerline_mode_checked = settings_.centerline_mode;
        if (ImGui::Checkbox("Construction##line_type", &construction_mode_checked) && construction_mode_checked) {
            settings_.construction_mode = true;
            settings_.centerline_mode = false;

        } else if (!construction_mode_checked) {
            settings_.construction_mode = false;
        }

        if (ImGui::Checkbox("Centerline##line_type", &centerline_mode_checked) && centerline_mode_checked) {
            settings_.centerline_mode = true;
            settings_.construction_mode = false;
        } else if (!centerline_mode_checked) {
            settings_.centerline_mode = false;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("Display");
        ImGui::Checkbox("Sketch Grid", &settings_.show_grid);
        ImGui::Checkbox("Snap", &settings_.show_snap);
        ImGui::Checkbox("Slice", &settings_.show_slice);
        ImGui::Checkbox("Profile", &settings_.show_profile);
        ImGui::Checkbox("Points", &settings_.show_points);
        ImGui::Checkbox("Dimensions", &settings_.show_dimensions);
        ImGui::Checkbox("Constraints", &settings_.show_constraints);
        ImGui::Checkbox("Projected Geometries", &settings_.show_projected_geometries);
        ImGui::Checkbox("Construction Geometries", &settings_.show_construction_geometries);
        ImGui::Checkbox("3D Sketch", &settings_.enable_3d_sketch);

        ImGui::EndChild();

        // Buttons at bottom
        ImGui::Separator();
        ImGui::BeginGroup();
        if (ImGui::Button("Finish Sketch", ImVec2(140, 0))) {
            finish_sketch_requested_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel Sketch", ImVec2(140, 0))) {
            cancel_sketch_requested_ = true;
        }
        ImGui::EndGroup();
    }
    ImGui::End();
}

} // namespace mtcad
