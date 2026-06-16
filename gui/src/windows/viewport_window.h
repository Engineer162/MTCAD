#ifndef MTCAD_GUI_WINDOWS_VIEWPORT_WINDOW_H
#define MTCAD_GUI_WINDOWS_VIEWPORT_WINDOW_H

#include "imgui.h"
#include <vector>

class ViewportWindow {
public:
    enum SketchPlane {
        SketchPlane_None = 0,
        SketchPlane_XY,
        SketchPlane_YZ,
        SketchPlane_XZ,
    };

    struct Vec3 {
        float x;
        float y;
        float z;
    };

    struct LineSegment {
        Vec3 start;
        Vec3 end;
    };

    struct SnapTarget {
        Vec3 world = {0.0f, 0.0f, 0.0f};
        bool valid = false;
        bool snapped_to_segment_interior = false;
        int segment_index = -1;
        float segment_t = 0.0f;
        bool snapped_to_geometry = false;
        bool snapped_to_grid = false;
    };

    void Render(const ImGuiIO& io);
    void SetPanButton(ImGuiMouseButton button);
    ImGuiMouseButton GetPanButton() const;
    void SetOrbitButton(ImGuiMouseButton button);
    ImGuiMouseButton GetOrbitButton() const;
    void SetSettingsIconTexture(ImTextureID texture_id);
    void SetCameraIconTexture(ImTextureID texture_id);
    void SetGridIconTexture(ImTextureID texture_id);
    void SetAwaitingSketchPlaneSelection(bool enabled);
    void SetSolidMode(bool enabled);
    void SetSketchMode(bool enabled, SketchPlane plane = SketchPlane_None);
    void FinishSketchMode();
    void CancelSketchMode();
    void SetLayoutGridVisible(bool enabled);
    void SetSketchGridVisible(bool enabled);
    void SetSnapToGridEnabled(bool enabled);
    bool ProjectToScreen(const Vec3& world, const ImVec2& canvas_pos, const ImVec2& canvas_size, ImVec2& screen) const;
    Vec3 ScreenToWorldPlane(const ImVec2& screen_pos, const ImVec2& canvas_pos, const ImVec2& canvas_size, SketchPlane plane) const;
    bool ConsumeSelectedSketchPlane(SketchPlane* out_plane);
    bool GetCanvasRect(ImVec2* out_pos, ImVec2* out_size) const;
    void StartLineDrawing();
    void StartRectangleDrawing();
    void StartCircleDrawing();
    void CancelLineDrawing();
    bool GetCommittedLineSegments(std::vector<LineSegment>* out_segments);
    bool GetSelectedFillProfiles(std::vector<int>* out_fill_ids, std::vector<std::vector<Vec3>>* out_polygons_world) const;
    bool GetSelectedPolygonWorldPoints(std::vector<Vec3>* out_points) const;
    bool GetSelectedFillIds(int* out_polygon_index, int* out_overlap_index) const;
    void SetExtrudedBodyPreview(const std::vector<Vec3>& polygon_points, float depth_world);
    void SetExtrudedBodyPreviewBatch(const std::vector<std::vector<Vec3>>& polygons, float depth_world);
    void ClearExtrudedBodyPreview();
    void SetExtrudedBodyFinal(const std::vector<Vec3>& polygon_points, float depth_world);
    void SetExtrudedBodyFinalBatch(const std::vector<std::vector<Vec3>>& polygons, float depth_world);
    void ClearExtrudedBodyFinal();

private:
    const char* MouseButtonName(ImGuiMouseButton button) const;
    Vec3 CameraPosition() const;
    Vec3 CameraForward() const;
    Vec3 CameraRight() const;
    Vec3 CameraUp() const;
    void DrawLine3D(ImDrawList* draw_list, const Vec3& a, const Vec3& b, ImU32 color, float thickness, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    void DrawPlaneQuad3D(ImDrawList* draw_list, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, ImU32 fill_color, ImU32 outline_color, float outline_thickness, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    bool PointInTriangle2D(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c) const;
    bool PointInQuad2D(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d) const;
    bool ProjectQuadToScreen(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const ImVec2& canvas_pos, const ImVec2& canvas_size, ImVec2& sa, ImVec2& sb, ImVec2& sc, ImVec2& sd) const;
    void AlignCameraToSketchPlane(SketchPlane plane);
    void DrawAdaptiveSketchGrid(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    void DrawCircle3D(ImDrawList* draw_list, const Vec3& center, float radius, ImU32 color, float thickness, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    Vec3 ScreenToWorldPlane(const ImVec2& screen_pos, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    Vec3 SnapToGrid(const Vec3& world_pos) const;
    SnapTarget ResolveSnapTarget(const ImVec2& mouse_screen, const ImVec2& canvas_pos, const ImVec2& canvas_size) const;
    void CommitLineSegment(const Vec3& start, const Vec3& end, const SnapTarget& start_snap, const SnapTarget& end_snap);
    void SplitSegmentAtPoint(int segment_index, const Vec3& point);

    enum class SketchToolMode {
        None = 0,
        Line,
        Rectangle,
        Circle,
    };

    Vec3 target_ = {0.0f, 0.0f, 0.0f};
    float yaw_ = 0.85f;
    float pitch_ = 0.65f;
    float distance_ = 26.0f;
    double info_last_scroll_time_ = -1000.0;
    ImGuiMouseButton pan_button_ = ImGuiMouseButton_Right;
    ImGuiMouseButton orbit_button_ = ImGuiMouseButton_Left;
    bool show_layout_grid_ = true;
    bool sketch_grid_visible_ = true;
    bool snap_to_grid_enabled_ = true;
    bool show_axes_ = true;
    bool show_hud_ = true;
    bool ortho_mode_ = false;
    bool persp_mode_ = true;
    bool persp_w_ortho_faces_mode_ = false;
    bool options_follow_viewport_ = true;
    bool options_snap_requested_ = true;
    ImGuiDir options_anchor_dir_ = ImGuiDir_Down;
    ImTextureID settings_icon_texture_ = (ImTextureID)0;
    ImTextureID camera_icon_texture_ = (ImTextureID)0;
    ImTextureID grid_icon_texture_ = (ImTextureID)0;
    bool awaiting_sketch_plane_selection_ = false;
    bool sketch_mode_active_ = false;
    bool crosshair_enabled_ = false;
    SketchPlane sketch_geometry_plane_ = SketchPlane_None;
    bool sketch_layout_grid_previous_visible_ = true;
    bool sketch_layout_grid_manual_override_ = false;
    SketchToolMode active_sketch_tool_ = SketchToolMode::None;
    SketchPlane active_sketch_plane_ = SketchPlane_None;
    SketchPlane selected_sketch_plane_ = SketchPlane_None;
    ImVec2 last_canvas_pos_ = ImVec2(0.0f, 0.0f);
    ImVec2 last_canvas_size_ = ImVec2(0.0f, 0.0f);

    // Line drawing state
    bool is_drawing_line_ = false;
    bool awaiting_line_start_ = false;
    Vec3 line_start_ = {0.0f, 0.0f, 0.0f};
    Vec3 line_end_ = {0.0f, 0.0f, 0.0f};
    SnapTarget line_start_snap_;
    SnapTarget line_end_snap_;
    int focused_line_field_ = 1; // 0 = angle, 1 = length (default length)
    bool is_drawing_rectangle_ = false;
    bool awaiting_rectangle_start_ = false;
    Vec3 rectangle_start_ = {0.0f, 0.0f, 0.0f};
    Vec3 rectangle_end_ = {0.0f, 0.0f, 0.0f};
    bool is_drawing_circle_ = false;
    bool awaiting_circle_start_ = false;
    Vec3 circle_center_ = {0.0f, 0.0f, 0.0f};
    Vec3 circle_edge_ = {0.0f, 0.0f, 0.0f};
    std::vector<LineSegment> committed_line_segments_;
    std::vector<int> segment_group_ids_; // Track which group each segment belongs to
    int next_segment_group_ = 0; // Next group ID to assign
    int current_line_group_id_ = -1; // group id for the line stroke in progress
    
    // Polygon interaction state
    int hovered_polygon_index_ = -1;
    int selected_polygon_index_ = -1;
    int hovered_overlap_index_ = -1;
    int selected_overlap_index_ = -1;
    struct SelectedFillRegion {
        int polygon_index = -1;
        int overlap_index = -1;
        int fill_id = -1;
        int selection_id = -1;
        std::vector<ImVec2> screen_points;
        std::vector<Vec3> world_points;
    };
    std::vector<SelectedFillRegion> selected_fill_regions_;
    int next_selected_fill_id_ = 1;
    std::vector<ImVec2> selected_polygon_points_;
    std::vector<Vec3> selected_polygon_world_points_;
    bool extruded_body_preview_visible_ = false;
    std::vector<Vec3> extruded_body_preview_polygon_;
    std::vector<std::vector<Vec3>> extruded_body_preview_polygons_;
    float extruded_body_preview_depth_world_ = 0.0f;
    bool extruded_body_final_visible_ = false;
    std::vector<Vec3> extruded_body_final_polygon_;
    std::vector<std::vector<Vec3>> extruded_body_final_polygons_;
    float extruded_body_final_depth_world_ = 0.0f;
};

#endif
