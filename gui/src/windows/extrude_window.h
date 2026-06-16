#pragma once

#include "imgui.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mtcad {

struct ExtrudePaletteSettings {
    float distance = 1.0f;
    float taper_angle_degrees = 0.0f;
    int direction = 0;   // 0 = One Side, 1 = Symmetric, 2 = Two Sides
    int operation = 0;   // 0 = Join, 1 = Cut, 2 = Intersect, 3 = New Body
    bool chain_faces = false;
    bool flip_direction = false;
};

struct ExtrudePoint3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class ExtrudePaletteWindow {
public:
    ExtrudePaletteWindow() = default;

    void Render(const ImVec2& viewport_pos, const ImVec2& viewport_size);
    void Open();
    void Close();
    bool IsOpen() const { return open_; }
    const ExtrudePaletteSettings& GetSettings() const { return settings_; }
    void AddSourceProfile(int fill_id, const std::vector<ExtrudePoint3D>& polygon_points);
    void ClearSourceProfiles();
    int GetSourceProfileCount() const;
    void SetSourcePolygonWorld(const std::vector<ExtrudePoint3D>& polygon_points);
    void SetSourceFillId(int fill_id);
    bool HasPreviewBodies() const { return !preview_body_polygons_world_.empty(); }
    const std::vector<std::vector<ExtrudePoint3D>>& GetPreviewBodyPolygonsWorld() const { return preview_body_polygons_world_; }
    bool HasPreviewBody() const { return preview_body_visible_; }
    const std::vector<ExtrudePoint3D>& GetPreviewBodyPolygonWorld() const { return preview_body_polygon_world_; }
    float GetPreviewBodyDepthWorld() const { return preview_body_depth_world_; }
    bool HasAppliedPreviewBodies() const { return !applied_preview_body_polygons_world_.empty(); }
    const std::vector<std::vector<ExtrudePoint3D>>& GetAppliedPreviewBodyPolygonsWorld() const { return applied_preview_body_polygons_world_; }
    float GetAppliedPreviewBodyDepthWorld() const { return applied_preview_body_depth_world_; }
    int GetSourceFillId() const { return source_fill_id_; }
    bool ConsumeApplyExtrude();
    bool ConsumeCancelExtrude();

private:
    struct JobState {
        bool running = false;
        bool completed = false;
        bool failed = false;
        size_t processed_bodies = 0;
        double total_volume_delta = 0.0;
        double total_surface_work = 0.0;
        std::string status_text;
    };

    void CaptureApplyPreviewBody();
    void StartKernelJob();

    bool open_ = false;
    bool apply_extrude_requested_ = false;
    bool cancel_extrude_requested_ = false;
    ImVec2 size_ = ImVec2(320, 320);
    float padding_ = 12.0f;
    ExtrudePaletteSettings settings_;
    int source_fill_id_ = -1;
    std::vector<int> source_fill_ids_;
    std::vector<std::vector<ExtrudePoint3D>> source_profile_polygons_world_points_;
    std::vector<ExtrudePoint3D> source_polygon_world_points_;
    std::vector<std::vector<ExtrudePoint3D>> preview_body_polygons_world_;
    std::vector<ExtrudePoint3D> preview_body_polygon_world_;
    float preview_body_depth_world_ = 0.0f;
    bool preview_body_visible_ = false;
    std::vector<std::vector<ExtrudePoint3D>> applied_preview_body_polygons_world_;
    float applied_preview_body_depth_world_ = 0.0f;
    std::shared_ptr<JobState> job_state_;
    std::mutex job_state_mutex_;

    void UpdatePreviewBody();
};

} // namespace mtcad