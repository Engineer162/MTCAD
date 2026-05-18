#include "toolbar_window.h"

#include <algorithm>
#include <cstring>

void ToolbarWindow::SetIconTexture(IconId icon_id, ImTextureID texture_id) {
    switch (icon_id) {
        case IconId::Extrude: extrude_icon_texture_ = texture_id; break;
        case IconId::Revolve: revolve_icon_texture_ = texture_id; break;
        case IconId::Hole: hole_icon_texture_ = texture_id; break;
        case IconId::PressPull: press_pull_icon_texture_ = texture_id; break;
        case IconId::Chamfer: chamfer_icon_texture_ = texture_id; break;
        case IconId::Fillet: fillet_icon_texture_ = texture_id; break;
        case IconId::Shell: shell_icon_texture_ = texture_id; break;
        case IconId::Combine: combine_icon_texture_ = texture_id; break;
        case IconId::SplitBody: split_body_icon_texture_ = texture_id; break;
        case IconId::Plane: plane_icon_texture_ = texture_id; break;
        case IconId::Axes: axes_icon_texture_ = texture_id; break;
        case IconId::Measure: measure_icon_texture_ = texture_id; break;
        case IconId::Section: section_icon_texture_ = texture_id; break;
        case IconId::Interference: interference_icon_texture_ = texture_id; break;
        case IconId::Point: point_icon_texture_ = texture_id; break;
        case IconId::Mirror: mirror_icon_texture_ = texture_id; break;
        case IconId::Line: line_icon_texture_ = texture_id; break;
        case IconId::Rectangle: rectangle_icon_texture_ = texture_id; break;
        case IconId::Circle: circle_icon_texture_ = texture_id; break;
        case IconId::Arc: arc_icon_texture_ = texture_id; break;
        case IconId::Text: text_icon_texture_ = texture_id; break;
        default: break;
    }
}

void ToolbarWindow::SetIconScale(float icon_scale) {
    icon_scale_ = icon_scale;
    if (icon_scale_ < 0.80f) {
        icon_scale_ = 0.80f;
    }
    if (icon_scale_ > 2.00f) {
        icon_scale_ = 2.00f;
    }
}

void ToolbarWindow::SetSketchMode(bool enabled) {
    sketch_mode_ = enabled;
    active_tool_name_ = nullptr;
}

bool ToolbarWindow::ConsumeBeginSketchRequest() {
    if (!begin_sketch_request_pending_) {
        return false;
    }
    begin_sketch_request_pending_ = false;
    return true;
}

bool ToolbarWindow::ConsumeBeginSolidModeRequest() {
    if (!begin_solid_mode_request_pending_) {
        return false;
    }
    begin_solid_mode_request_pending_ = false;
    return true;
}

bool ToolbarWindow::ConsumeSelectedTool(const char** out_tool_name) {
    if (selected_tool_name_pending_ == nullptr) {
        return false;
    }
    if (out_tool_name != nullptr) {
        *out_tool_name = selected_tool_name_pending_;
    }
    selected_tool_name_pending_ = nullptr;
    return true;
}

void ToolbarWindow::Render(const ImGuiIO& io) {
    struct ToolEntry {
        const char* label;
    };

    struct PopupToolEntry {
        const char* label;
        bool separator_before;
        enum Action {
            Action_SelectTool,
            Action_SwitchToSketchMode,
            Action_SwitchToSolidMode,
        } action = Action_SelectTool;
    };

    enum ToolCategoryId {
        ToolCategory_Create = 0,
        ToolCategory_Modify,
        ToolCategory_Construct,
        ToolCategory_Inspect,
    };

    struct ToolCategory {
        ToolCategoryId id;
        const char* title;
        const ToolEntry* tools;
        int tool_count;
        const PopupToolEntry* popup_tools;
        int popup_tool_count;
    };

    // Solid mode tool definitions
    static const ToolEntry tools_create[] = {
        {"Extrude"},
        {"Revolve"},
        {"Hole"},
    };

    static const ToolEntry tools_modify[] = {
        {"Press Pull"},
        {"Fillet"},
        {"Chamfer"},
        {"Shell"},
        {"Combine"},
        {"Split Body"},
    };

    static const ToolEntry tools_construct[] = {
        {"Plane"},
        {"Axes"},
        {"Point"},
    };

    static const ToolEntry tools_inspect[] = {
        {"Measure"},
        {"Section"},
        {"Interference"},
    };

    static const PopupToolEntry tools_create_popup[] = {
        {"Create Sketch", false, PopupToolEntry::Action_SwitchToSketchMode},
        {"Create Form", false, PopupToolEntry::Action_SelectTool},
        {"Derive", false, PopupToolEntry::Action_SelectTool},

        {"Extrude", true, PopupToolEntry::Action_SelectTool},
        {"Revolve", false, PopupToolEntry::Action_SelectTool},
        {"Sweep", false, PopupToolEntry::Action_SelectTool},
        {"Loft", false, PopupToolEntry::Action_SelectTool},
        {"Rib", false, PopupToolEntry::Action_SelectTool},
        {"Web", false, PopupToolEntry::Action_SelectTool},
        {"Emboss", false, PopupToolEntry::Action_SelectTool},

        {"Hole", true, PopupToolEntry::Action_SelectTool},
        {"Thread", false, PopupToolEntry::Action_SelectTool},

        {"Box", true, PopupToolEntry::Action_SelectTool},
        {"Cylinder", false, PopupToolEntry::Action_SelectTool},
        {"Sphere", false, PopupToolEntry::Action_SelectTool},
        {"Torus", false, PopupToolEntry::Action_SelectTool},
        {"Coil", false, PopupToolEntry::Action_SelectTool},
        {"pipe", false, PopupToolEntry::Action_SelectTool},

        {"Pattern", true, PopupToolEntry::Action_SelectTool},
        {"Mirror", false, PopupToolEntry::Action_SelectTool},

        {"Thicken", true, PopupToolEntry::Action_SelectTool},
        {"Boundary Fill", false, PopupToolEntry::Action_SelectTool},

        {"Create Base Feature", true, PopupToolEntry::Action_SelectTool},
        {"Joint Origin", false, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry tools_modify_popup[] = {
        {"Press Pull", false, PopupToolEntry::Action_SelectTool},

        {"Fillet", true, PopupToolEntry::Action_SelectTool},
        {"Chamfer", false, PopupToolEntry::Action_SelectTool},

        {"Shell", true, PopupToolEntry::Action_SelectTool},
        {"Draft", false, PopupToolEntry::Action_SelectTool},
        {"Scale", false, PopupToolEntry::Action_SelectTool},
        {"Combine", false, PopupToolEntry::Action_SelectTool},
        {"Offset Face", false, PopupToolEntry::Action_SelectTool},
        {"Replace Face", false, PopupToolEntry::Action_SelectTool},
        {"Split Face", false, PopupToolEntry::Action_SelectTool},
        {"Split Body", false, PopupToolEntry::Action_SelectTool},
        {"Silhouette Split", false, PopupToolEntry::Action_SelectTool},

        {"Move/Copy", true, PopupToolEntry::Action_SelectTool},
        {"Align", false, PopupToolEntry::Action_SelectTool},
        {"Delete", false, PopupToolEntry::Action_SelectTool},
        {"Remove", false, PopupToolEntry::Action_SelectTool},

        {"Simplify", true, PopupToolEntry::Action_SelectTool},

        {"Physical Material", true, PopupToolEntry::Action_SelectTool},
        {"Appearance", false, PopupToolEntry::Action_SelectTool},
        {"Volumetric Lattice", false, PopupToolEntry::Action_SelectTool},
        {"Manage Materials", false, PopupToolEntry::Action_SelectTool},

        {"Change Parameters", true, PopupToolEntry::Action_SelectTool},
        {"Compute All", false, PopupToolEntry::Action_SelectTool},

        {"Bill of Materials (BOM)", true, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry tools_construct_popup[] = {
        {"Offset Plane", false, PopupToolEntry::Action_SelectTool},
        {"Plane at Angle", false, PopupToolEntry::Action_SelectTool},
        {"Tangent Plane", false, PopupToolEntry::Action_SelectTool},
        {"Midplane", false, PopupToolEntry::Action_SelectTool},
        {"Plane Through Two Edges", false, PopupToolEntry::Action_SelectTool},
        {"Plane Through Three Points", false, PopupToolEntry::Action_SelectTool},
        {"Plane Along Path", false, PopupToolEntry::Action_SelectTool},

        {"Axis Through Cylinder/Cone/torus", true, PopupToolEntry::Action_SelectTool},
        {"Axis Perpendicular To Face", false, PopupToolEntry::Action_SelectTool},
        {"Axis Through Two Planes", false, PopupToolEntry::Action_SelectTool},
        {"Axis Through Two Points", false, PopupToolEntry::Action_SelectTool},
        {"Axis Through Edge", false, PopupToolEntry::Action_SelectTool},

        {"Point At Vertex", true, PopupToolEntry::Action_SelectTool},
        {"Point Through Two Edges", false, PopupToolEntry::Action_SelectTool},
        {"Point Through Three Planes", false, PopupToolEntry::Action_SelectTool},
        {"Point at Center Of Circle/Sphere/Torus", false, PopupToolEntry::Action_SelectTool},
        {"Point At Edge And Plane", false, PopupToolEntry::Action_SelectTool},
        {"Point Along Path", false, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry tools_inspect_popup[] = {
        {"Measure", false, PopupToolEntry::Action_SelectTool},
        {"Interference", false, PopupToolEntry::Action_SelectTool},

        {"Curvature Comb Analysis", true, PopupToolEntry::Action_SelectTool},
        {"Zebra Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Draft Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Curvature Map Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Isocurve Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Accessibility Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Minimum Radius Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Section Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Center of Mass", false, PopupToolEntry::Action_SelectTool},

        {"Display Component Colors", true, PopupToolEntry::Action_SelectTool},
        {"Display Mesh Face Groups", false, PopupToolEntry::Action_SelectTool},
    };

    // Sketch mode tool definitions
    static const ToolEntry sketch_tools_create[] = {
        {"Line"},
        {"Rectangle"},
        {"Circle"},
    };

    static const ToolEntry sketch_tools_modify[] = {
        {"Trim"},
        {"Extend"},
        {"Offset"},
    };

    static const ToolEntry sketch_tools_construct[] = {
        {"Centerline"},
        {"Point"},
        {"Polygon"},
    };

    static const ToolEntry sketch_tools_inspect[] = {
        {"Dimension"},
        {"Constraint"},
        {"Profile"},
    };

    static const PopupToolEntry sketch_tools_create_popup[] = {
        {"Finish Sketch", false, PopupToolEntry::Action_SwitchToSolidMode},
        {"Line", true, PopupToolEntry::Action_SelectTool},
        {"Midpoint Line", false, PopupToolEntry::Action_SelectTool},
        {"Rectangle", false, PopupToolEntry::Action_SelectTool},
        {"Circle", false, PopupToolEntry::Action_SelectTool},
        {"Arc", false, PopupToolEntry::Action_SelectTool},
        {"Polygon", false, PopupToolEntry::Action_SelectTool},
        {"Ellipse", false, PopupToolEntry::Action_SelectTool},
        {"Slot", false, PopupToolEntry::Action_SelectTool},
        {"Spline", false, PopupToolEntry::Action_SelectTool},
        {"Conic Curve", false, PopupToolEntry::Action_SelectTool},
        {"Point", false, PopupToolEntry::Action_SelectTool},
        {"Text", false, PopupToolEntry::Action_SelectTool},

        {"Mirror", true, PopupToolEntry::Action_SelectTool},
        {"Circular Pattern", false, PopupToolEntry::Action_SelectTool},
        {"Rectangular Pattern", false, PopupToolEntry::Action_SelectTool},

        {"Project/Include", true, PopupToolEntry::Action_SelectTool},

        {"Sketch Dimension", true, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry sketch_tools_modify_popup[] = {
        {"Fillet", false, PopupToolEntry::Action_SelectTool},
        {"Chamfer", false, PopupToolEntry::Action_SelectTool},
        {"Blend Curve", false, PopupToolEntry::Action_SelectTool},
        {"Offset", false, PopupToolEntry::Action_SelectTool},

        {"Trim", true, PopupToolEntry::Action_SelectTool},
        {"Extend", false, PopupToolEntry::Action_SelectTool},
        {"Break", false, PopupToolEntry::Action_SelectTool},
        {"Sketch Scale", false, PopupToolEntry::Action_SelectTool},
        {"Move/Copy", false, PopupToolEntry::Action_SelectTool},

        {"Change Parameters", true, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry sketch_tools_constraints_popup[] = {
        {"AutoConstrain", false, PopupToolEntry::Action_SelectTool},
        {"AutoConstrain from datum", false, PopupToolEntry::Action_SelectTool},
        {"Horizontal/Vertical", false, PopupToolEntry::Action_SelectTool},
        {"Coincident", true, PopupToolEntry::Action_SelectTool},
        {"Tangent", false, PopupToolEntry::Action_SelectTool},
        {"Equal", false, PopupToolEntry::Action_SelectTool},
        {"Parallel", false, PopupToolEntry::Action_SelectTool},
        {"Perpendicular", false, PopupToolEntry::Action_SelectTool},
        {"Fix/Unfix", false, PopupToolEntry::Action_SelectTool},
        {"MidPoint", false, PopupToolEntry::Action_SelectTool},
        {"Concentric", false, PopupToolEntry::Action_SelectTool},
        {"Colinear", false, PopupToolEntry::Action_SelectTool},
        {"Symmetry", false, PopupToolEntry::Action_SelectTool},
        {"Curvature", false, PopupToolEntry::Action_SelectTool},
        {"Polygon", false, PopupToolEntry::Action_SelectTool},
    };

    static const PopupToolEntry sketch_tools_inspect_popup[] = {
        {"Measure", false, PopupToolEntry::Action_SelectTool},
        {"Interference", false, PopupToolEntry::Action_SelectTool},

        {"Curvature Comb Analysis", true, PopupToolEntry::Action_SelectTool},
        {"Zebra Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Draft Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Curvature Map Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Isocurve Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Accessibility Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Minimum Radius Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Section Analysis", false, PopupToolEntry::Action_SelectTool},
        {"Center of Mass", false, PopupToolEntry::Action_SelectTool},

        {"Display Component Colors", true, PopupToolEntry::Action_SelectTool},
        {"Display Mesh Face Groups", false, PopupToolEntry::Action_SelectTool},
    };

    // Solid mode categories
    static const ToolCategory categories[] = {
        {
            ToolCategory_Create,
            "Create",
            tools_create,
            IM_ARRAYSIZE(tools_create),
            tools_create_popup,
            IM_ARRAYSIZE(tools_create_popup),
        },
        {
            ToolCategory_Modify,
            "Modify",
            tools_modify,
            IM_ARRAYSIZE(tools_modify),
            tools_modify_popup,
            IM_ARRAYSIZE(tools_modify_popup),
        },
        {
            ToolCategory_Construct,
            "Construct",
            tools_construct,
            IM_ARRAYSIZE(tools_construct),
            tools_construct_popup,
            IM_ARRAYSIZE(tools_construct_popup),
        },
        {
            ToolCategory_Inspect,
            "Inspect",
            tools_inspect,
            IM_ARRAYSIZE(tools_inspect),
            tools_inspect_popup,
            IM_ARRAYSIZE(tools_inspect_popup),
        },
    };

    // Sketch mode categories
    static const ToolCategory sketch_categories[] = {
        {
            ToolCategory_Create,
            "Create",
            sketch_tools_create,
            IM_ARRAYSIZE(sketch_tools_create),
            sketch_tools_create_popup,
            IM_ARRAYSIZE(sketch_tools_create_popup),
        },
        {
            ToolCategory_Modify,
            "Modify",
            sketch_tools_modify,
            IM_ARRAYSIZE(sketch_tools_modify),
            sketch_tools_modify_popup,
            IM_ARRAYSIZE(sketch_tools_modify_popup),
        },
        {
            ToolCategory_Construct,
            "Construct",
            sketch_tools_construct,
            IM_ARRAYSIZE(sketch_tools_construct),
            sketch_tools_constraints_popup,
            IM_ARRAYSIZE(sketch_tools_constraints_popup),
        },
        {
            ToolCategory_Inspect,
            "Inspect",
            sketch_tools_inspect,
            IM_ARRAYSIZE(sketch_tools_inspect),
            sketch_tools_inspect_popup,
            IM_ARRAYSIZE(sketch_tools_inspect_popup),
        },
    };

    // Change this list to control category placement order in the toolbar UI.
    static const ToolCategoryId category_order[] = {
        ToolCategory_Create,
        ToolCategory_Modify,
        ToolCategory_Construct,
        ToolCategory_Inspect,
    };

    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Toolbar");

    float titlebar_clearance = 4.0f + (io.FontGlobalScale - 1.0f) * ImGui::GetFontSize() * 0.5f;
    if (titlebar_clearance < 4.0f) {
        titlebar_clearance = 4.0f;
    }
    ImGui::Dummy(ImVec2(0.0f, titlebar_clearance));

    ImGui::TextUnformatted(sketch_mode_ ? "Sketch tools" : "Modeling tools");
    ImGui::Separator();

    const ToolCategory* active_categories = sketch_mode_ ? sketch_categories : categories;
    const int active_category_count = sketch_mode_ ? IM_ARRAYSIZE(sketch_categories) : IM_ARRAYSIZE(categories);

    const float tool_button_width = 48.0f * icon_scale_;
    const float tool_button_height = 48.0f * icon_scale_;

    // Set the respective icon for each tool button based on its label

    // Icons for solid tools
    auto resolve_tool_icon = [&](const char* tool_label) {
        if (std::strcmp(tool_label, "Extrude") == 0) {
            return extrude_icon_texture_;
        }
        if (std::strcmp(tool_label, "Revolve") == 0) {
            return revolve_icon_texture_;
        }
        if (std::strcmp(tool_label, "Hole") == 0) {
            return hole_icon_texture_;
        }
        if (std::strcmp(tool_label, "Press Pull") == 0) {
            return press_pull_icon_texture_;
        }
        if (std::strcmp(tool_label, "Chamfer") == 0) {
            return chamfer_icon_texture_;
        }
        if (std::strcmp(tool_label, "Fillet") == 0) {
            return fillet_icon_texture_;
        }
        if (std::strcmp(tool_label, "Shell") == 0) {
            return shell_icon_texture_;
        }
        if (std::strcmp(tool_label, "Combine") == 0) {
            return combine_icon_texture_;
        }
        if (std::strcmp(tool_label, "Split Body") == 0) {
            return split_body_icon_texture_;
        }
        if (std::strcmp(tool_label, "Plane") == 0) {
            return plane_icon_texture_;
        }
        if (std::strcmp(tool_label, "Axes") == 0) {
            return axes_icon_texture_;
        }
        if (std::strcmp(tool_label, "Measure") == 0) {
            return measure_icon_texture_;
        }
        if (std::strcmp(tool_label, "Section") == 0) {
            return section_icon_texture_;
        }
        if (std::strcmp(tool_label, "Interference") == 0) {
            return interference_icon_texture_;
        }

        // Shared icons
        if (std::strcmp(tool_label, "Point") == 0) {
            return point_icon_texture_;
        }
        if (std::strcmp(tool_label, "Mirror") == 0) {
            return mirror_icon_texture_;
        }

        // Icons for sketch tools
        if (std::strcmp(tool_label, "Line") == 0) {
            return line_icon_texture_;
        }
        if (std::strcmp(tool_label, "Rectangle") == 0) {
            return rectangle_icon_texture_;
        }
        if (std::strcmp(tool_label, "Circle") == 0) {
            return circle_icon_texture_;
        }
        if (std::strcmp(tool_label, "Arc") == 0) {
            return arc_icon_texture_;
        }
        if (std::strcmp(tool_label, "Text") == 0) {
            return text_icon_texture_;
        }

        return (ImTextureID)0;
    };

    auto render_category = [&](const ToolCategory& category, float block_width) {
        for (int i = 0; i < category.tool_count; ++i) {
            const ToolEntry& tool = category.tools[i];
            const bool is_active_tool = (active_tool_name_ != nullptr && active_tool_name_ == tool.label);
            if (is_active_tool) {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(67, 92, 124, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 108, 145, 255));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(90, 120, 160, 255));
            }

            bool clicked = false;
            const ImTextureID tool_icon = resolve_tool_icon(tool.label);
            if (tool_icon != (ImTextureID)0) {
                ImGui::PushID(tool.label);
                clicked = ImGui::Button("##tool_icon_button", ImVec2(tool_button_width, tool_button_height));
                const ImVec2 button_min = ImGui::GetItemRectMin();
                const ImVec2 button_max = ImGui::GetItemRectMax();
                const float button_size = (std::min)(tool_button_width, tool_button_height);
                float icon_size = button_size * icon_scale_;
                if (icon_size > button_size - 4.0f) {
                    icon_size = button_size - 4.0f;
                }
                if (icon_size < 12.0f) {
                    icon_size = 12.0f;
                }
                const ImVec2 icon_pos(
                    button_min.x + (tool_button_width - icon_size) * 0.5f,
                    button_min.y + (tool_button_height - icon_size) * 0.5f);
                ImGui::GetWindowDrawList()->AddImage(
                    tool_icon,
                    icon_pos,
                    ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size));
                ImGui::PopID();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", tool.label);
                }
            } else {
                clicked = ImGui::Button(tool.label, ImVec2(tool_button_width, tool_button_height));
            }

            if (clicked) {
                active_tool_name_ = tool.label;
                selected_tool_name_pending_ = tool.label;
            }

            if (is_active_tool) {
                ImGui::PopStyleColor(3);
            }

            if (i + 1 < category.tool_count) {
                ImGui::SameLine();
            }
        }

        ImGui::Spacing();
        const ImVec2 title_size = ImGui::CalcTextSize(category.title);
        const float triangle_size = ImGui::GetFontSize() * 0.42f;
        const float triangle_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float dropdown_label_width = title_size.x + triangle_spacing + triangle_size;
        float centered_x = (block_width - dropdown_label_width) * 0.5f;
        if (centered_x < 0.0f) {
            centered_x = 0.0f;
        }
        ImGui::SetCursorPosX(centered_x);

        const ImVec2 click_size(dropdown_label_width, title_size.y);
        if (ImGui::InvisibleButton("category_dropdown_label", click_size)) {
            ImGui::OpenPopup("category_more_tools_popup");
        }

        const ImVec2 item_min = ImGui::GetItemRectMin();
        const ImVec2 item_max = ImGui::GetItemRectMax();
        const bool label_hovered = ImGui::IsItemHovered();
        const bool label_held = ImGui::IsItemActive();
        const bool popup_open = ImGui::IsPopupOpen("category_more_tools_popup");
        const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
        ImDrawList* label_draw_list = ImGui::GetWindowDrawList();

        const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
        const ImVec2 bg_min(item_min.x - frame_padding.x * 0.7f, item_min.y - frame_padding.y * 0.35f);
        const ImVec2 bg_max(item_max.x + frame_padding.x * 0.7f, item_max.y + frame_padding.y * 0.35f);
        ImU32 bg_color = 0;
        if (label_held || popup_open) {
            bg_color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        } else if (label_hovered) {
            bg_color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        }
        if (bg_color != 0) {
            label_draw_list->AddRectFilled(bg_min, bg_max, bg_color, ImGui::GetStyle().FrameRounding);
        }

        label_draw_list->AddText(item_min, text_color, category.title);

        const float tri_center_x = item_min.x + title_size.x + triangle_spacing + triangle_size * 0.5f;
        const float tri_center_y = item_min.y + title_size.y * 0.55f;
        label_draw_list->AddTriangleFilled(
            ImVec2(tri_center_x - triangle_size * 0.5f, tri_center_y - triangle_size * 0.35f),
            ImVec2(tri_center_x + triangle_size * 0.5f, tri_center_y - triangle_size * 0.35f),
            ImVec2(tri_center_x, tri_center_y + triangle_size * 0.45f),
            ImGui::GetColorU32(ImGuiCol_Text));

        if (label_hovered) {
            ImGui::SetTooltip("More %s tools", category.title);
        }

        if (ImGui::BeginPopup("category_more_tools_popup")) {
            ImGui::TextUnformatted(category.title);
            ImGui::Separator();
            for (int i = 0; i < category.popup_tool_count; ++i) {
                const PopupToolEntry& popup_tool = category.popup_tools[i];
                if (popup_tool.separator_before) {
                    ImGui::Separator();
                }

                const ImTextureID popup_tool_icon = resolve_tool_icon(popup_tool.label);
                if (popup_tool_icon != (ImTextureID)0) {
                    const float popup_icon_size = ImGui::GetTextLineHeight();
                    ImGui::Image(popup_tool_icon, ImVec2(popup_icon_size, popup_icon_size));
                    ImGui::SameLine();
                }

                if (ImGui::Selectable(popup_tool.label, active_tool_name_ != nullptr && active_tool_name_ == popup_tool.label)) {
                    if (popup_tool.action == PopupToolEntry::Action_SwitchToSketchMode) {
                        begin_sketch_request_pending_ = true;
                    } else if (popup_tool.action == PopupToolEntry::Action_SwitchToSolidMode) {
                        begin_solid_mode_request_pending_ = true;
                        sketch_mode_ = false;
                        active_tool_name_ = nullptr;
                    } else {
                        active_tool_name_ = popup_tool.label;
                        selected_tool_name_pending_ = popup_tool.label;
                    }
                }
            }
            ImGui::EndPopup();
        }
    };

    ImGui::BeginChild("toolbar_category_row", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for (int order_idx = 0; order_idx < IM_ARRAYSIZE(category_order); ++order_idx) {
        const ToolCategoryId target_id = category_order[order_idx];
        const ToolCategory* category_to_render = nullptr;
        for (int category_idx = 0; category_idx < active_category_count; ++category_idx) {
            if (active_categories[category_idx].id == target_id) {
                category_to_render = &active_categories[category_idx];
                break;
            }
        }
        if (category_to_render == nullptr) {
            continue;
        }

        if (order_idx > 0) {
            ImGui::SameLine();
        }

        const float item_spacing_x = ImGui::GetStyle().ItemSpacing.x;
        const float tools_row_width = category_to_render->tool_count * tool_button_width + (category_to_render->tool_count - 1) * item_spacing_x;
        const float title_width = ImGui::CalcTextSize(category_to_render->title).x + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFontSize() * 0.42f;
        const float category_block_width = (tools_row_width > title_width) ? tools_row_width : title_width;

        ImGui::PushID(order_idx);
        ImGui::BeginChild("category_block", ImVec2(category_block_width, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
        render_category(*category_to_render, category_block_width);
        ImGui::EndChild();
        const ImVec2 block_min = ImGui::GetItemRectMin();
        const ImVec2 block_max = ImGui::GetItemRectMax();
        ImGui::PopID();

        if (order_idx + 1 < IM_ARRAYSIZE(category_order)) {
            const float separator_x = block_max.x + ImGui::GetStyle().ItemSpacing.x * 0.5f;
            draw_list->AddLine(
                ImVec2(separator_x, block_min.y),
                ImVec2(separator_x, block_max.y),
                ImGui::GetColorU32(ImGuiCol_Separator),
                1.0f);
        }
    }
    ImGui::EndChild();

    //ImGui::Separator();
    //ImGui::TextUnformatted("Construction Options");
    //ImGui::Checkbox("Snap to Grid", &snap_to_grid_);
    //ImGui::SliderFloat("Grid Step", &grid_step_, 0.1f, 10.0f, "%.2f");

    //ImGui::Separator();
    //ImGui::Text("Active Tool: %s", active_tool_name_ ? active_tool_name_ : "None");

    ImGui::End();
}
