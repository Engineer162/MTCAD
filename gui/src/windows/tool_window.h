#pragma once
#include "imgui.h"

namespace mtcad {

struct SketchPaletteSettings {
    // Line type
    bool construction_mode = false;
    bool centerline_mode = false;
    
    // Display toggles
    bool show_grid = true;
    bool show_snap = true;
    bool show_slice = false;
    bool show_profile = true;
    bool show_points = true;
    bool show_dimensions = true;
    bool show_constraints = true;
    bool show_projected_geometries = true;
    bool show_construction_geometries = true;
    bool enable_3d_sketch = false;
};

class SketchPaletteWindow {
public:
    SketchPaletteWindow() = default;
    void Render(const ImVec2& viewport_pos, const ImVec2& viewport_size);
    void Open();
    void Close();
    bool IsOpen() const { return open_; }
    const SketchPaletteSettings& GetSettings() const { return settings_; }
    bool ConsumeFinishSketch();
    bool ConsumeCancelSketch();

private:
    bool open_ = false;
    bool finish_sketch_requested_ = false;
    bool cancel_sketch_requested_ = false;
    ImVec2 size_ = ImVec2(300, 420);
    float padding_ = 12.0f;
    SketchPaletteSettings settings_;
};

} // namespace mtcad
