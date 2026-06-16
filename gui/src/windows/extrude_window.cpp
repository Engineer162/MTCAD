#include "extrude_window.h"

#include "mtcad/kernel.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace mtcad {

void ExtrudePaletteWindow::Open() {
    open_ = true;
}

void ExtrudePaletteWindow::Close() {
    open_ = false;
}

void ExtrudePaletteWindow::AddSourceProfile(int fill_id, const std::vector<ExtrudePoint3D>& polygon_points) {
    if (fill_id < 0 || polygon_points.size() < 3) {
        return;
    }

    source_fill_id_ = fill_id;
    source_polygon_world_points_ = polygon_points;

    for (size_t i = 0; i < source_fill_ids_.size(); ++i) {
        if (source_fill_ids_[i] == fill_id) {
            source_profile_polygons_world_points_[i] = polygon_points;
            return;
        }
    }

    source_fill_ids_.push_back(fill_id);
    source_profile_polygons_world_points_.push_back(polygon_points);
}

void ExtrudePaletteWindow::ClearSourceProfiles() {
    source_fill_id_ = -1;
    source_fill_ids_.clear();
    source_polygon_world_points_.clear();
    source_profile_polygons_world_points_.clear();
    preview_body_polygons_world_.clear();
    preview_body_polygon_world_.clear();
    preview_body_depth_world_ = 0.0f;
    preview_body_visible_ = false;
    applied_preview_body_polygons_world_.clear();
    applied_preview_body_depth_world_ = 0.0f;
}

int ExtrudePaletteWindow::GetSourceProfileCount() const {
    return (int)source_profile_polygons_world_points_.size();
}

void ExtrudePaletteWindow::SetSourcePolygonWorld(const std::vector<ExtrudePoint3D>& polygon_points) {
    source_polygon_world_points_ = polygon_points;
    source_profile_polygons_world_points_.clear();
    source_fill_ids_.clear();
    if (polygon_points.size() >= 3) {
        source_profile_polygons_world_points_.push_back(polygon_points);
        if (source_fill_id_ >= 0) {
            source_fill_ids_.push_back(source_fill_id_);
        }
    }
}

void ExtrudePaletteWindow::SetSourceFillId(int fill_id) {
    source_fill_id_ = fill_id;
}

void ExtrudePaletteWindow::UpdatePreviewBody() {
    preview_body_polygons_world_.clear();
    for (const auto& profile : source_profile_polygons_world_points_) {
        if (profile.size() >= 3) {
            preview_body_polygons_world_.push_back(profile);
        }
    }

    if (!preview_body_polygons_world_.empty()) {
        preview_body_polygon_world_ = preview_body_polygons_world_.front();
        preview_body_depth_world_ = std::max(0.0f, settings_.distance);
        preview_body_visible_ = true;
    } else {
        preview_body_polygon_world_.clear();
        preview_body_depth_world_ = 0.0f;
        preview_body_visible_ = false;
    }
}

void ExtrudePaletteWindow::CaptureApplyPreviewBody() {
    applied_preview_body_polygons_world_.clear();
    for (const auto& polygon : preview_body_polygons_world_) {
        if (polygon.size() >= 3) {
            applied_preview_body_polygons_world_.push_back(polygon);
        }
    }
    applied_preview_body_depth_world_ = preview_body_depth_world_;
}

bool ExtrudePaletteWindow::ConsumeApplyExtrude() {
    if (!apply_extrude_requested_) {
        return false;
    }
    apply_extrude_requested_ = false;
    return true;
}

bool ExtrudePaletteWindow::ConsumeCancelExtrude() {
    if (!cancel_extrude_requested_) {
        return false;
    }
    cancel_extrude_requested_ = false;
    return true;
}

void ExtrudePaletteWindow::StartKernelJob() {
    UpdatePreviewBody();

    ExtrudePaletteSettings settings = settings_;

    auto state = std::make_shared<JobState>();
    {
        std::lock_guard<std::mutex> lock(job_state_mutex_);
        job_state_ = state;
        job_state_->running = true;
        job_state_->status_text = "Submitting extrude job...";
    }

    std::thread([this, state, settings]() {
        try {
            std::vector<mtcad_kernel_extrude_body_input> inputs(source_profile_polygons_world_points_.size());
            if (inputs.empty()) {
                std::lock_guard<std::mutex> lock(job_state_mutex_);
                if (job_state_ == state) {
                    state->completed = true;
                    state->running = false;
                    state->status_text = "No selected profiles to extrude";
                }
                return;
            }

            std::vector<mtcad_kernel_extrude_body_result> results(inputs.size());

            auto profile_area = [](const std::vector<ExtrudePoint3D>& poly) {
                if (poly.size() < 3) {
                    return 0.0;
                }
                double area = 0.0;
                for (size_t i = 0; i < poly.size(); ++i) {
                    const auto& a = poly[i];
                    const auto& b = poly[(i + 1) % poly.size()];
                    area += (double)a.x * (double)b.y - (double)b.x * (double)a.y;
                }
                return std::abs(area) * 0.5;
            };

            for (size_t i = 0; i < inputs.size(); ++i) {
                inputs[i].body_id = (int)i;
                inputs[i].profile_area = profile_area(source_profile_polygons_world_points_[i]);
                inputs[i].depth = settings.distance;
                inputs[i].taper_angle_degrees = settings.taper_angle_degrees;
                inputs[i].operation = settings.operation;
            }

            const size_t processed = mtcad_kernel_extrude_cut_parallel(
                inputs.data(),
                inputs.size(),
                results.data(),
                results.size(),
                0);

            double total_volume_delta = 0.0;
            double total_surface_work = 0.0;
            for (size_t i = 0; i < processed; ++i) {
                total_volume_delta += results[i].estimated_volume_delta;
                total_surface_work += results[i].estimated_surface_work;
            }

            {
                std::lock_guard<std::mutex> lock(job_state_mutex_);
                if (job_state_ == state) {
                    state->processed_bodies = processed;
                    state->total_volume_delta = total_volume_delta;
                    state->total_surface_work = total_surface_work;
                    state->completed = true;
                    state->running = false;
                    state->status_text = "Kernel finished: " + std::to_string(processed) + " body processed";
                }
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(job_state_mutex_);
            if (job_state_ == state) {
                state->failed = true;
                state->running = false;
                state->status_text = "Kernel job failed";
            }
        }
    }).detach();
}

void ExtrudePaletteWindow::Render(const ImVec2& viewport_pos, const ImVec2& viewport_size) {
    if (!open_) {
        return;
    }

    ImVec2 pos = ImVec2(
        viewport_pos.x + viewport_size.x - size_.x - padding_,
        viewport_pos.y + viewport_size.y * 0.5f - size_.y * 0.5f
    );

    ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(size_, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Extrude", nullptr, 0)) {
        ImGui::BeginChild("extrude_palette_content", ImVec2(0, -50), false);

        ImGui::TextUnformatted("Profile");
        if (!source_fill_ids_.empty()) {
            ImGui::Text("Selected fills: %d", (int)source_fill_ids_.size());
            ImGui::Text("Active fill ID: %d", source_fill_id_);
        } else {
            ImGui::TextUnformatted("Selected fills: none");
        }
        if (ImGui::Button("Clear Selected Fills")) {
            ClearSourceProfiles();
        }
        ImGui::Checkbox("Chain Faces", &settings_.chain_faces);
        ImGui::Checkbox("Flip Direction", &settings_.flip_direction);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("Extent");
        ImGui::InputFloat("Distance", &settings_.distance, 0.1f, 1.0f, "%.3f");
        if (settings_.distance < 0.0f) {
            settings_.distance = 0.0f;
        }
        ImGui::InputFloat("Taper Angle", &settings_.taper_angle_degrees, 0.1f, 1.0f, "%.2f");

        static const char* direction_items[] = {"One Side", "Symmetric", "Two Sides"};
        ImGui::Combo("Direction", &settings_.direction, direction_items, IM_ARRAYSIZE(direction_items));

        static const char* operation_items[] = {"Join", "Cut", "Intersect", "New Body"};
        ImGui::Combo("Operation", &settings_.operation, operation_items, IM_ARRAYSIZE(operation_items));

        ImGui::EndChild();

        UpdatePreviewBody();

        ImGui::Separator();
        ImGui::BeginGroup();
        if (ImGui::Button("Apply Extrude", ImVec2(140, 0))) {
            CaptureApplyPreviewBody();
            apply_extrude_requested_ = true;
            StartKernelJob();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(140, 0))) {
            cancel_extrude_requested_ = true;
        }
        ImGui::EndGroup();

        {
            std::lock_guard<std::mutex> lock(job_state_mutex_);
            if (job_state_ != nullptr) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextUnformatted(job_state_->status_text.c_str());
                if (job_state_->completed) {
                    ImGui::Text("Bodies: %zu", job_state_->processed_bodies);
                    ImGui::Text("Volume delta: %.3f", job_state_->total_volume_delta);
                    ImGui::Text("Surface work: %.3f", job_state_->total_surface_work);
                }
                if (!preview_body_visible_ && source_profile_polygons_world_points_.empty()) {
                    ImGui::TextUnformatted("Select a closed profile in the viewport first.");
                }
            }
        }
    }
    ImGui::End();
}

} // namespace mtcad