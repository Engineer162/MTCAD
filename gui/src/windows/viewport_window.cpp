#include "viewport_window.h"

#include <cmath>
#include <cstdio>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <string>

namespace {
constexpr float Pi = 3.14159265f;
constexpr float kHalfPi = (Pi / 2);

static ViewportWindow::Vec3 Add(const ViewportWindow::Vec3& a, const ViewportWindow::Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static ViewportWindow::Vec3 Sub(const ViewportWindow::Vec3& a, const ViewportWindow::Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static ViewportWindow::Vec3 Mul(const ViewportWindow::Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

static float Dot(const ViewportWindow::Vec3& a, const ViewportWindow::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static ViewportWindow::Vec3 Cross(const ViewportWindow::Vec3& a, const ViewportWindow::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static ViewportWindow::Vec3 Normalize(const ViewportWindow::Vec3& v) {
    const float len_sq = Dot(v, v);
    if (len_sq < 1e-8f) {
        return {0.0f, 0.0f, 0.0f};
    }
    const float inv_len = 1.0f / std::sqrt(len_sq);
    return {v.x * inv_len, v.y * inv_len, v.z * inv_len};
}

static float DistSq(const ViewportWindow::Vec3& a, const ViewportWindow::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx*dx + dy*dy + dz*dz;
}

static ImVec2 ProjectPlanePoint(const ViewportWindow::Vec3& p, ViewportWindow::SketchPlane plane) {
    switch (plane) {
        case ViewportWindow::SketchPlane_XY:
            return ImVec2(p.x, p.y);
        case ViewportWindow::SketchPlane_YZ:
            return ImVec2(p.y, p.z);
        case ViewportWindow::SketchPlane_XZ:
            return ImVec2(p.x, p.z);
        case ViewportWindow::SketchPlane_None:
        default:
            return ImVec2(p.x, p.y);
    }
}

static ViewportWindow::Vec3 UnprojectPlanePoint(const ImVec2& p, ViewportWindow::SketchPlane plane) {
    switch (plane) {
        case ViewportWindow::SketchPlane_XY:
            return {p.x, p.y, 0.0f};
        case ViewportWindow::SketchPlane_YZ:
            return {0.0f, p.x, p.y};
        case ViewportWindow::SketchPlane_XZ:
            return {p.x, 0.0f, p.y};
        case ViewportWindow::SketchPlane_None:
        default:
            return {p.x, p.y, 0.0f};
    }
}

static ImVec2 ClosestPointOnSegment2D(const ImVec2& p, const ImVec2& a, const ImVec2& b, float* out_t = nullptr) {
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float apx = p.x - a.x;
    const float apy = p.y - a.y;
    const float denom = abx * abx + aby * aby;
    float t = 0.0f;
    if (denom > 1e-8f) {
        t = (apx * abx + apy * aby) / denom;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    if (out_t != nullptr) {
        *out_t = t;
    }
    return ImVec2(a.x + abx * t, a.y + aby * t);
}

static bool PointInTriangle2D_local(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c) {
    const float v0x = c.x - a.x;
    const float v0y = c.y - a.y;
    const float v1x = b.x - a.x;
    const float v1y = b.y - a.y;
    const float v2x = p.x - a.x;
    const float v2y = p.y - a.y;

    const float dot00 = v0x * v0x + v0y * v0y;
    const float dot01 = v0x * v1x + v0y * v1y;
    const float dot02 = v0x * v2x + v0y * v2y;
    const float dot11 = v1x * v1x + v1y * v1y;
    const float dot12 = v1x * v2x + v1y * v2y;

    const float denom = dot00 * dot11 - dot01 * dot01;
    if (std::fabs(denom) < 1e-8f) return false;
    const float inv_denom = 1.0f / denom;
    const float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    const float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
    return (u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f);
}

static float PolygonSignedArea2D(const std::vector<ImVec2>& pts) {
    if (pts.size() < 3) {
        return 0.0f;
    }

    double area = 0.0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const ImVec2& a = pts[i];
        const ImVec2& b = pts[(i + 1) % pts.size()];
        area += (double)a.x * (double)b.y - (double)b.x * (double)a.y;
    }
    return (float)(0.5 * area);
}

static bool IsConvexPolygon2D(const std::vector<ImVec2>& pts) {
    if (pts.size() < 3) {
        return false;
    }

    const float orientation = PolygonSignedArea2D(pts);
    if (std::fabs(orientation) < 1e-6f) {
        return false;
    }

    const float expected_sign = orientation > 0.0f ? 1.0f : -1.0f;
    for (size_t i = 0; i < pts.size(); ++i) {
        const ImVec2& a = pts[i];
        const ImVec2& b = pts[(i + 1) % pts.size()];
        const ImVec2& c = pts[(i + 2) % pts.size()];
        const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::fabs(cross) < 1e-6f) {
            continue;
        }
        if (cross * expected_sign < 0.0f) {
            return false;
        }
    }

    return true;
}

static ImVec2 IntersectSegmentWithLine2D(const ImVec2& s, const ImVec2& e, const ImVec2& a, const ImVec2& b) {
    const float se_x = e.x - s.x;
    const float se_y = e.y - s.y;
    const float ab_x = b.x - a.x;
    const float ab_y = b.y - a.y;
    const float denom = se_x * ab_y - se_y * ab_x;
    if (std::fabs(denom) < 1e-8f) {
        return e;
    }

    const float t = ((a.x - s.x) * ab_y - (a.y - s.y) * ab_x) / denom;
    return {s.x + t * se_x, s.y + t * se_y};
}

static std::vector<ImVec2> ClipPolygonAgainstConvexClip2D(const std::vector<ImVec2>& subject, const std::vector<ImVec2>& clip) {
    std::vector<ImVec2> output = subject;
    if (output.size() < 3 || clip.size() < 3) {
        return {};
    }

    const float clip_area = PolygonSignedArea2D(clip);
    if (std::fabs(clip_area) < 1e-6f) {
        return {};
    }
    const bool clip_ccw = clip_area > 0.0f;

    auto inside = [clip_ccw](const ImVec2& p, const ImVec2& a, const ImVec2& b) {
        const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        return clip_ccw ? (cross >= -1e-6f) : (cross <= 1e-6f);
    };

    for (size_t i = 0; i < clip.size(); ++i) {
        const ImVec2& clip_a = clip[i];
        const ImVec2& clip_b = clip[(i + 1) % clip.size()];
        std::vector<ImVec2> input = output;
        output.clear();

        if (input.empty()) {
            break;
        }

        ImVec2 prev = input.back();
        bool prev_inside = inside(prev, clip_a, clip_b);
        for (const ImVec2& curr : input) {
            const bool curr_inside = inside(curr, clip_a, clip_b);
            if (curr_inside) {
                if (!prev_inside) {
                    output.push_back(IntersectSegmentWithLine2D(prev, curr, clip_a, clip_b));
                }
                output.push_back(curr);
            } else if (prev_inside) {
                output.push_back(IntersectSegmentWithLine2D(prev, curr, clip_a, clip_b));
            }
            prev = curr;
            prev_inside = curr_inside;
        }

        auto same_point = [](const ImVec2& lhs, const ImVec2& rhs) {
            const float dx = lhs.x - rhs.x;
            const float dy = lhs.y - rhs.y;
            return (dx * dx + dy * dy) <= 1e-6f;
        };

        while (output.size() > 1 && same_point(output.front(), output.back())) {
            output.pop_back();
        }

        if (output.size() < 3) {
            return {};
        }
    }

    return output;
}

// Triangulate polygon defined by 2D points using ear clipping. Returns list of triangle index triples.
static std::vector<std::array<int,3>> TriangulatePolygon(const std::vector<ImVec2>& pts) {
    std::vector<std::array<int,3>> tris;
    const int n = (int)pts.size();
    if (n < 3) return tris;
    std::vector<int> V(n);
    for (int i=0;i<n;++i) V[i]=i;

    const float area = PolygonSignedArea2D(pts);
    if (std::fabs(area) < 1e-6f) {
        return tris;
    }
    const float winding_sign = area > 0.0f ? 1.0f : -1.0f;

    auto is_convex = [&](int i0, int i1, int i2)->bool{
        const ImVec2& a = pts[i0];
        const ImVec2& b = pts[i1];
        const ImVec2& c = pts[i2];
        const float cross = (b.x - a.x)*(c.y - a.y) - (b.y - a.y)*(c.x - a.x);
        return cross * winding_sign > 1e-6f;
    };

    int guard = 0;
    while ((int)V.size() > 3 && guard < 10000) {
        bool clipped = false;
        const int m = (int)V.size();
        for (int vi=0; vi<m; ++vi) {
            int i_prev = V[(vi + m - 1) % m];
            int i_curr = V[vi];
            int i_next = V[(vi + 1) % m];
            if (!is_convex(i_prev, i_curr, i_next)) continue;
            bool any_inside = false;
            for (int k=0;k<m;++k) {
                int idx = V[k];
                if (idx==i_prev||idx==i_curr||idx==i_next) continue;
                if (PointInTriangle2D_local(pts[idx], pts[i_prev], pts[i_curr], pts[i_next])) { any_inside=true; break; }
            }
            if (any_inside) continue;
            tris.push_back(std::array<int,3>{i_prev, i_curr, i_next});
            V.erase(V.begin()+vi);
            clipped = true;
            break;
        }
        if (!clipped) break;
        guard++;
    }
    if ((int)V.size() == 3) {
        tris.push_back(std::array<int,3>{V[0], V[1], V[2]});
    }
    return tris;
}

static ViewportWindow::SketchPlane InferSketchPlaneFromWorldPolygon(const std::vector<ViewportWindow::Vec3>& polygon) {
    if (polygon.size() < 3) {
        return ViewportWindow::SketchPlane_None;
    }

    const float epsilon = 1e-4f;
    const float first_x = polygon.front().x;
    const float first_y = polygon.front().y;
    const float first_z = polygon.front().z;

    bool same_x = true;
    bool same_y = true;
    bool same_z = true;
    for (const auto& point : polygon) {
        same_x = same_x && (std::fabs(point.x - first_x) <= epsilon);
        same_y = same_y && (std::fabs(point.y - first_y) <= epsilon);
        same_z = same_z && (std::fabs(point.z - first_z) <= epsilon);
    }

    if (same_z) return ViewportWindow::SketchPlane_XY;
    if (same_x) return ViewportWindow::SketchPlane_YZ;
    if (same_y) return ViewportWindow::SketchPlane_XZ;
    return ViewportWindow::SketchPlane_None;
}

static std::vector<ImVec2> ProjectPolygonToScreen(const std::vector<ViewportWindow::Vec3>& polygon, const ImVec2& canvas_pos, const ImVec2& canvas_size, const ViewportWindow* viewport) {
    std::vector<ImVec2> projected;
    projected.reserve(polygon.size());
    for (const auto& point : polygon) {
        ImVec2 screen;
        if (!viewport->ProjectToScreen(point, canvas_pos, canvas_size, screen)) {
            return {};
        }
        projected.push_back(screen);
    }
    return projected;
}

static void DrawExtrudedBodyPreview(ImDrawList* draw_list, const std::vector<ViewportWindow::Vec3>& polygon, float depth_world, const ViewportWindow* viewport, const ImVec2& canvas_pos, const ImVec2& canvas_size, bool solid_render, const ViewportWindow::Vec3& camera_pos, const ViewportWindow::Vec3& camera_forward) {
    if (draw_list == nullptr || viewport == nullptr || polygon.size() < 3) {
        return;
    }

    if (depth_world <= 0.0f) {
        std::vector<ImVec2> base_screen = ProjectPolygonToScreen(polygon, canvas_pos, canvas_size, viewport);
        if (base_screen.size() < 3) {
            return;
        }

        const ImU32 flat_fill = solid_render ? IM_COL32(128, 132, 138, 255) : IM_COL32(112, 156, 220, 72);
        const ImU32 flat_outline = solid_render ? IM_COL32(92, 96, 102, 255) : IM_COL32(190, 220, 255, 190);
        draw_list->AddConvexPolyFilled(base_screen.data(), (int)base_screen.size(), flat_fill);
        draw_list->AddPolyline(base_screen.data(), (int)base_screen.size(), flat_outline, ImDrawFlags_Closed, 2.0f);
        return;
    }

    const ViewportWindow::SketchPlane plane = InferSketchPlaneFromWorldPolygon(polygon);
    if (plane == ViewportWindow::SketchPlane_None) {
        return;
    }

    auto offset_point = [&](const ViewportWindow::Vec3& p) {
        switch (plane) {
            case ViewportWindow::SketchPlane_XY: return ViewportWindow::Vec3{p.x, p.y, p.z + depth_world};
            case ViewportWindow::SketchPlane_YZ: return ViewportWindow::Vec3{p.x + depth_world, p.y, p.z};
            case ViewportWindow::SketchPlane_XZ: return ViewportWindow::Vec3{p.x, p.y + depth_world, p.z};
            case ViewportWindow::SketchPlane_None:
            default: return p;
        }
    };

    std::vector<ViewportWindow::Vec3> offset_polygon;
    offset_polygon.reserve(polygon.size());
    for (const auto& point : polygon) {
        offset_polygon.push_back(offset_point(point));
    }

    std::vector<ImVec2> base_screen = ProjectPolygonToScreen(polygon, canvas_pos, canvas_size, viewport);
    std::vector<ImVec2> offset_screen = ProjectPolygonToScreen(offset_polygon, canvas_pos, canvas_size, viewport);
    if (base_screen.size() < 3 || offset_screen.size() < 3) {
        return;
    }

    const ImU32 side_fill = solid_render ? IM_COL32(120, 124, 130, 255) : IM_COL32(86, 118, 168, 100);
    const ImU32 top_fill = solid_render ? IM_COL32(142, 146, 152, 255) : IM_COL32(112, 156, 220, 120);
    const ImU32 bottom_fill = top_fill;
    const ImU32 outline = solid_render ? IM_COL32(74, 78, 84, 255) : IM_COL32(190, 220, 255, 180);
    constexpr float kCapFacingThreshold = 0.10f;

    struct FaceItem {
        std::vector<ImVec2> pts;
        float depth_key = 0.0f;
        ImU32 fill = 0;
    };

    struct LineItem {
        std::vector<ImVec2> pts;
        float depth_key = 0.0f;
        float thickness = 1.0f;
        ImU32 color = 0;
    };

    auto tri_depth = [&](const ViewportWindow::Vec3& w0, const ViewportWindow::Vec3& w1, const ViewportWindow::Vec3& w2) {
        const ViewportWindow::Vec3 centroid = {
            (w0.x + w1.x + w2.x) * (1.0f / 3.0f),
            (w0.y + w1.y + w2.y) * (1.0f / 3.0f),
            (w0.z + w1.z + w2.z) * (1.0f / 3.0f)
        };
        return Dot(Sub(centroid, camera_pos), camera_forward);
    };

    auto quad_depth = [&](const ViewportWindow::Vec3& w0, const ViewportWindow::Vec3& w1, const ViewportWindow::Vec3& w2, const ViewportWindow::Vec3& w3) {
        const ViewportWindow::Vec3 centroid = {
            (w0.x + w1.x + w2.x + w3.x) * 0.25f,
            (w0.y + w1.y + w2.y + w3.y) * 0.25f,
            (w0.z + w1.z + w2.z + w3.z) * 0.25f
        };
        return Dot(Sub(centroid, camera_pos), camera_forward);
    };

    std::vector<FaceItem> faces;
    std::vector<LineItem> outlines;
    faces.reserve(base_screen.size() * 2 + base_screen.size() * 2);
    outlines.reserve(base_screen.size() * 2 + base_screen.size() * 2);

    // Side walls are convex quads, so keep each wall as a single face.
    for (size_t i = 0; i < base_screen.size(); ++i) {
        const size_t j = (i + 1) % base_screen.size();
        const ViewportWindow::Vec3& wa = polygon[i];
        const ViewportWindow::Vec3& wb = polygon[j];
        const ViewportWindow::Vec3& wc = offset_polygon[j];
        const ViewportWindow::Vec3& wd = offset_polygon[i];
        const float side_depth = quad_depth(wa, wb, wc, wd);
        const ViewportWindow::Vec3 side_centroid = {
            (wa.x + wb.x + wc.x + wd.x) * 0.25f,
            (wa.y + wb.y + wc.y + wd.y) * 0.25f,
            (wa.z + wb.z + wc.z + wd.z) * 0.25f
        };
        const ViewportWindow::Vec3 side_normal = Cross(Sub(wb, wa), Sub(wd, wa));
        const bool side_visible = Dot(side_normal, Sub(camera_pos, side_centroid)) < 0.0f;

        faces.push_back({
            {base_screen[i], base_screen[j], offset_screen[j], offset_screen[i]},
            side_depth,
            side_fill
        });
        if (side_visible) {
            outlines.push_back({
                {base_screen[i], base_screen[j], offset_screen[j], offset_screen[i]},
                side_depth,
                1.0f,
                outline
            });
        }
    }

    auto polygon_normal = [&](const std::vector<ViewportWindow::Vec3>& poly) {
        ViewportWindow::Vec3 normal = {0.0f, 0.0f, 0.0f};
        for (size_t i = 0; i < poly.size(); ++i) {
            const ViewportWindow::Vec3& curr = poly[i];
            const ViewportWindow::Vec3& next = poly[(i + 1) % poly.size()];
            normal.x += (curr.y - next.y) * (curr.z + next.z);
            normal.y += (curr.z - next.z) * (curr.x + next.x);
            normal.z += (curr.x - next.x) * (curr.y + next.y);
        }
        return normal;
    };

    ViewportWindow::Vec3 top_centroid = {0.0f, 0.0f, 0.0f};
    for (const auto& point : polygon) {
        top_centroid.x += point.x;
        top_centroid.y += point.y;
        top_centroid.z += point.z;
    }
    const float top_inv_n = 1.0f / (float)polygon.size();
    top_centroid.x *= top_inv_n;
    top_centroid.y *= top_inv_n;
    top_centroid.z *= top_inv_n;

    ViewportWindow::Vec3 bottom_centroid = {0.0f, 0.0f, 0.0f};
    for (const auto& point : offset_polygon) {
        bottom_centroid.x += point.x;
        bottom_centroid.y += point.y;
        bottom_centroid.z += point.z;
    }
    const float bottom_inv_n = 1.0f / (float)offset_polygon.size();
    bottom_centroid.x *= bottom_inv_n;
    bottom_centroid.y *= bottom_inv_n;
    bottom_centroid.z *= bottom_inv_n;

    const ViewportWindow::Vec3 prism_center = {
        (top_centroid.x + bottom_centroid.x) * 0.5f,
        (top_centroid.y + bottom_centroid.y) * 0.5f,
        (top_centroid.z + bottom_centroid.z) * 0.5f
    };

    const ViewportWindow::Vec3 cap_normal = polygon_normal(polygon);
    const ViewportWindow::Vec3 extrusion_dir = Normalize(Sub(bottom_centroid, top_centroid));
    ViewportWindow::Vec3 oriented_cap_normal = cap_normal;
    if (Dot(oriented_cap_normal, extrusion_dir) < 0.0f) {
        oriented_cap_normal = Mul(oriented_cap_normal, -1.0f);
    }

    const float top_facing = Dot(oriented_cap_normal, Sub(camera_pos, top_centroid));
    const float bottom_facing = Dot(Mul(oriented_cap_normal, -1.0f), Sub(camera_pos, bottom_centroid));

    auto add_cap_faces = [&](const std::vector<ImVec2>& screen_pts, const ViewportWindow::Vec3& cap_depth_center, bool is_front_cap) {
        const float cap_depth = Dot(Sub(cap_depth_center, camera_pos), camera_forward);
        const bool visible = is_front_cap ? (top_facing < -kCapFacingThreshold) : (bottom_facing < -kCapFacingThreshold);
        if (!visible) {
            return;
        }
        if (IsConvexPolygon2D(screen_pts)) {
            faces.push_back({screen_pts, cap_depth, top_fill});
            outlines.push_back({screen_pts, cap_depth, 2.0f, outline});
            return;
        }

        const std::vector<std::array<int, 3>> cap_tris = TriangulatePolygon(screen_pts);
        for (const auto& tri : cap_tris) {
            faces.push_back({
                {screen_pts[(size_t)tri[0]], screen_pts[(size_t)tri[1]], screen_pts[(size_t)tri[2]]},
                cap_depth,
                top_fill
            });
        }
        outlines.push_back({screen_pts, cap_depth, 2.0f, outline});
    };

    add_cap_faces(base_screen, top_centroid, true);
    add_cap_faces(offset_screen, bottom_centroid, false);

    // Painter's algorithm: sort all faces by camera depth, keeping each face grouped.
    std::stable_sort(faces.begin(), faces.end(), [](const FaceItem& a, const FaceItem& b) {
        return a.depth_key > b.depth_key;
    });

    for (const auto& face : faces) {
        if (face.pts.size() == 3) {
            draw_list->AddTriangleFilled(face.pts[0], face.pts[1], face.pts[2], face.fill);
        } else if (face.pts.size() >= 3) {
            draw_list->AddConvexPolyFilled(face.pts.data(), (int)face.pts.size(), face.fill);
        }
    }

    std::stable_sort(outlines.begin(), outlines.end(), [](const LineItem& a, const LineItem& b) {
        return a.depth_key > b.depth_key;
    });

    for (const auto& line : outlines) {
        if (line.pts.size() >= 2) {
            draw_list->AddPolyline(line.pts.data(), (int)line.pts.size(), line.color, ImDrawFlags_Closed, line.thickness);
        }
    }

}

static ImVec2 OffsetPreviewPoint(const ImVec2& p, float depth_pixels) {
    return ImVec2(p.x + depth_pixels * 0.55f, p.y - depth_pixels * 0.55f);
}

static void DrawExtrudedBodyPreview(ImDrawList* draw_list, const std::vector<ImVec2>& polygon, float depth_pixels) {
    if (draw_list == nullptr || polygon.size() < 3) {
        return;
    }

    std::vector<ImVec2> offset_polygon;
    offset_polygon.reserve(polygon.size());
    for (const ImVec2& point : polygon) {
        offset_polygon.push_back(OffsetPreviewPoint(point, depth_pixels));
    }

    const ImU32 side_fill = IM_COL32(86, 118, 168, 100);
    const ImU32 top_fill = IM_COL32(112, 156, 220, 120);
    const ImU32 outline = IM_COL32(190, 220, 255, 180);

    for (size_t i = 0; i < polygon.size(); ++i) {
        const ImVec2& a = polygon[i];
        const ImVec2& b = polygon[(i + 1) % polygon.size()];
        const ImVec2& c = offset_polygon[(i + 1) % offset_polygon.size()];
        const ImVec2& d = offset_polygon[i];
        ImVec2 quad[4] = {a, b, c, d};
        draw_list->AddConvexPolyFilled(quad, 4, side_fill);
        draw_list->AddPolyline(quad, 4, outline, ImDrawFlags_Closed, 1.0f);
    }

    if (IsConvexPolygon2D(polygon)) {
        draw_list->AddConvexPolyFilled(polygon.data(), (int)polygon.size(), top_fill);
    } else {
        const std::vector<std::array<int, 3>> top_tris = TriangulatePolygon(polygon);
        for (const auto& tri : top_tris) {
            const ImVec2 tri_pts[3] = {polygon[(size_t)tri[0]], polygon[(size_t)tri[1]], polygon[(size_t)tri[2]]};
            draw_list->AddTriangleFilled(tri_pts[0], tri_pts[1], tri_pts[2], top_fill);
        }
    }

    if (IsConvexPolygon2D(offset_polygon)) {
        draw_list->AddConvexPolyFilled(offset_polygon.data(), (int)offset_polygon.size(), IM_COL32(68, 92, 138, 115));
    } else {
        const std::vector<std::array<int, 3>> bottom_tris = TriangulatePolygon(offset_polygon);
        for (const auto& tri : bottom_tris) {
            const ImVec2 tri_pts[3] = {offset_polygon[(size_t)tri[0]], offset_polygon[(size_t)tri[1]], offset_polygon[(size_t)tri[2]]};
            draw_list->AddTriangleFilled(tri_pts[0], tri_pts[1], tri_pts[2], IM_COL32(68, 92, 138, 115));
        }
    }

    draw_list->AddPolyline(polygon.data(), (int)polygon.size(), outline, ImDrawFlags_Closed, 2.0f);
    draw_list->AddPolyline(offset_polygon.data(), (int)offset_polygon.size(), outline, ImDrawFlags_Closed, 2.0f);
}

// Build closed polygon loops from committed line segments (2D screen space) using "directed-edge" walks.
static std::vector<std::vector<ImVec2>> BuildPolygonsFromSegments2D(const std::vector<std::pair<ImVec2,ImVec2>>& segs) {
    std::vector<std::vector<ImVec2>> polygons;
    if (segs.empty()) return polygons;

    const float eps2 = 1e-6f; // essentially zero, only exact vertex matches
    std::vector<ImVec2> verts;
    std::vector<std::pair<int,int>> edges;

    auto find_or_add = [&](const ImVec2& v)->int{
        for (int i=0;i<(int)verts.size();++i) {
            const float dx = verts[i].x - v.x;
            const float dy = verts[i].y - v.y;
            if (dx*dx + dy*dy <= eps2) return i;
        }
        verts.push_back(v);
        return (int)verts.size()-1;
    };

    for (const auto &s : segs) {
        int a = find_or_add(s.first);
        int b = find_or_add(s.second);
        edges.emplace_back(a,b);
    }

    std::vector<std::vector<int>> adj(verts.size());
    for (auto &e: edges) {
        adj[e.first].push_back(e.second);
        adj[e.second].push_back(e.first);
    }

    auto edge_key = [](int a, int b) {
        return std::make_pair(a, b);
    };

    auto normalize_loop = [&](const std::vector<int>& loop)->std::vector<int>{
        std::vector<int> cleaned;
        cleaned.reserve(loop.size());

        for (int idx : loop) {
            if (cleaned.empty() || cleaned.back() != idx) {
                cleaned.push_back(idx);
            }
        }

        while (cleaned.size() > 1 && cleaned.front() == cleaned.back()) {
            cleaned.pop_back();
        }

        if (cleaned.size() < 3) {
            return {};
        }

        std::vector<int> simplified;
        simplified.reserve(cleaned.size());
        const float collinear_eps = 1e-3f;

        for (size_t i = 0; i < cleaned.size(); ++i) {
            const int prev = cleaned[(i + cleaned.size() - 1) % cleaned.size()];
            const int curr = cleaned[i];
            const int next = cleaned[(i + 1) % cleaned.size()];
            const ImVec2& a = verts[prev];
            const ImVec2& b = verts[curr];
            const ImVec2& c = verts[next];
            const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (std::fabs(cross) <= collinear_eps && cleaned.size() > 3) {
                continue;
            }
            simplified.push_back(curr);
        }

        if (simplified.size() < 3) {
            return {};
        }

        return simplified;
    };

    auto canonical_loop_key = [&](const std::vector<int>& loop)->std::string{
        // normalize by rotating to smallest index
        int n = (int)loop.size();
        int best_i = 0;
        for (int i=1;i<n;++i) if (loop[i] < loop[best_i]) best_i = i;
        std::string a,b;
        for (int i=0;i<n;++i) {
            int v = loop[(best_i + i) % n];
            a += std::to_string(v) + ",";
        }
        for (int i=0;i<n;++i) {
            int v = loop[(best_i - i + n) % n];
            b += std::to_string(v) + ",";
        }
        return a < b ? a : b;
    };

    std::set<std::string> found_keys;
    std::set<std::pair<int, int>> visited_half_edges;

    const float two_pi = 6.283185307179586f;
    for (const auto &e : edges) {
        // try both directed edges
        for (int dir=0; dir<2; ++dir) {
            int u = (dir==0) ? e.first : e.second;
            int v = (dir==0) ? e.second : e.first;
            if (visited_half_edges.count(edge_key(u, v)) != 0) {
                continue;
            }
            std::vector<int> loop;
            loop.push_back(u);
            loop.push_back(v);
            int prev = u;
            int curr = v;
            int guard = 0;
            bool closed = false;
            while (guard++ < 10000) {
                const auto &neis = adj[curr];
                if (neis.empty()) break;
                // incoming vector
                ImVec2 vin = ImVec2(curr==prev ? 0.0f : verts[curr].x - verts[prev].x, curr==prev ? 0.0f : verts[curr].y - verts[prev].y);
                float best_ang = 1e9f; int best_nb = -1;
                for (int nb : neis) {
                    if (nb == prev) continue;
                    ImVec2 vout = ImVec2(verts[nb].x - verts[curr].x, verts[nb].y - verts[curr].y);
                    const float cross = vin.x * vout.y - vin.y * vout.x;
                    const float dot = vin.x * vout.x + vin.y * vout.y;
                    float ang = std::atan2(cross, dot);
                    if (ang < 0) ang += two_pi;
                    if (ang < best_ang) { best_ang = ang; best_nb = nb; }
                }
                if (best_nb == -1) break;
                prev = curr;
                curr = best_nb;
                loop.push_back(curr);
                if (curr == u) { // closed
                    closed = true;
                    break;
                }
            }

            if (!closed) {
                continue;
            }

            for (size_t i = 0; i + 1 < loop.size(); ++i) {
                visited_half_edges.insert(edge_key(loop[i], loop[i + 1]));
            }

            std::vector<int> loop_copy = normalize_loop(loop);
            if (loop_copy.size() < 3) {
                continue;
            }

            std::string key = canonical_loop_key(loop_copy);
            if (!found_keys.insert(key).second) {
                continue;
            }

            // compute polygon area in screen-space
            double area = 0.0;
            for (size_t ii=0; ii<loop_copy.size(); ++ii) {
                int ia = loop_copy[ii];
                int ib = loop_copy[(ii+1) % loop_copy.size()];
                area += (double)verts[ia].x * (double)verts[ib].y - (double)verts[ib].x * (double)verts[ia].y;
            }
            area = std::abs(area) * 0.5;
            const double min_area = 36.0; // pixels^2, might require adjustment
            if (area >= min_area) {
                polygons.emplace_back();
                for (int idx : loop_copy) polygons.back().push_back(verts[idx]);
            }
        }
    }

    return polygons;
}
}

const char* ViewportWindow::MouseButtonName(ImGuiMouseButton button) const {
    switch (button) {
        case ImGuiMouseButton_Left: return "Left";
        case ImGuiMouseButton_Right: return "Right";
        case ImGuiMouseButton_Middle: return "Middle";
        default: return "Mouse";
    }
}

void ViewportWindow::SetDragButton(ImGuiMouseButton button) {
    drag_button_ = button;
}

ImGuiMouseButton ViewportWindow::GetDragButton() const {
    return drag_button_;
}

void ViewportWindow::SetOrbitButton(ImGuiMouseButton button) {
    orbit_button_ = button;
}

ImGuiMouseButton ViewportWindow::GetOrbitButton() const {
    return orbit_button_;
}

void ViewportWindow::SetSettingsIconTexture(ImTextureID texture_id) {
    settings_icon_texture_ = texture_id;
}

void ViewportWindow::SetCameraIconTexture(ImTextureID texture_id) {
    camera_icon_texture_ = texture_id;
}

void ViewportWindow::SetGridIconTexture(ImTextureID texture_id) {
    grid_icon_texture_ = texture_id;
}

void ViewportWindow::SetAwaitingSketchPlaneSelection(bool enabled) {
    awaiting_sketch_plane_selection_ = enabled;
    if (enabled) {
        selected_sketch_plane_ = SketchPlane_None;
    }
}

void ViewportWindow::SetSketchMode(bool enabled, SketchPlane plane) {
    if (enabled) {
        if (!sketch_mode_active_) {
            sketch_layout_grid_previous_visible_ = show_layout_grid_;
            sketch_layout_grid_manual_override_ = false;
            show_layout_grid_ = false;
        }
        sketch_mode_active_ = true;
        if (plane != SketchPlane_None) {
            active_sketch_plane_ = plane;
            sketch_geometry_plane_ = plane; // Added sketch geometry plane
        }
        ortho_mode_ = true;
        persp_mode_ = false;
        persp_w_ortho_faces_mode_ = false;
        return;
    }

    CancelSketchMode();
}

void ViewportWindow::FinishSketchMode() {
    if (!sketch_mode_active_) {
        return;
    }

    sketch_mode_active_ = false;
    if (!sketch_layout_grid_manual_override_) {
        show_layout_grid_ = sketch_layout_grid_previous_visible_;
    }
    sketch_layout_grid_manual_override_ = false;
    active_sketch_plane_ = SketchPlane_None;
    CancelLineDrawing();
    hovered_polygon_index_ = -1;
    selected_polygon_index_ = -1;
    hovered_overlap_index_ = -1;
    selected_overlap_index_ = -1;
}

void ViewportWindow::CancelSketchMode() {
    if (!sketch_mode_active_) {
        return;
    }

    sketch_mode_active_ = false;
    if (!sketch_layout_grid_manual_override_) {
        show_layout_grid_ = sketch_layout_grid_previous_visible_;
    }
    sketch_layout_grid_manual_override_ = false;
    active_sketch_plane_ = SketchPlane_None;
    CancelLineDrawing();
    committed_line_segments_.clear();
    segment_group_ids_.clear();
    next_segment_group_ = 0;
    hovered_polygon_index_ = -1;
    selected_polygon_index_ = -1;
    hovered_overlap_index_ = -1;
    selected_overlap_index_ = -1;
    sketch_geometry_plane_ = SketchPlane_None;
}

void ViewportWindow::SetLayoutGridVisible(bool enabled) {
    if (sketch_mode_active_ && enabled != show_layout_grid_) {
        sketch_layout_grid_manual_override_ = true;
    }
    show_layout_grid_ = enabled;
}

void ViewportWindow::SetSolidMode(bool enabled) {
    if (enabled) {
        ortho_mode_ = false;
        persp_mode_ = true; 
        persp_w_ortho_faces_mode_ = false;
    }
}

bool ViewportWindow::ConsumeSelectedSketchPlane(SketchPlane* out_plane) {
    if (out_plane == nullptr) {
        return false;
    }
    if (selected_sketch_plane_ == SketchPlane_None) {
        return false;
    }
    *out_plane = selected_sketch_plane_;
    selected_sketch_plane_ = SketchPlane_None;
    return true;
}

namespace {

static float NiceGridStep(float desired_step) {
    if (desired_step <= 0.0f) {
        return 1.0f;
    }

    const float exponent = std::floor(std::log10(desired_step));
    const float base = std::pow(10.0f, exponent);
    const float normalized = desired_step / base;
    float step = 1.0f;
    if (normalized < 1.5f) {
        step = 1.0f;
    } else if (normalized < 3.5f) {
        step = 2.0f;
    } else if (normalized < 7.5f) {
        step = 5.0f;
    } else {
        step = 10.0f;
    }
    return step * base;
}

} // namespace

void ViewportWindow::DrawAdaptiveSketchGrid(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) const {
    if (draw_list == nullptr) {
        return;
    }

    const float world_per_pixel = (distance_ * 1.8f) / (canvas_size.y > 0.0f ? canvas_size.y : 1.0f);
    const float desired_step = world_per_pixel * 48.0f;
    const float step = NiceGridStep(desired_step);
    const float half_extent = world_per_pixel * ((canvas_size.x > canvas_size.y ? canvas_size.x : canvas_size.y) * 0.7f) + step * 4.0f;

    Vec3 center = target_;
    switch (active_sketch_plane_) {
        case SketchPlane_XY:
            center.z = 0.0f;
            break;
        case SketchPlane_YZ:
            center.x = 0.0f;
            break;
        case SketchPlane_XZ:
            center.y = 0.0f;
            break;
        case SketchPlane_None:
        default:
            return;
    }

    const ImU32 major_color = IM_COL32(76, 118, 154, 150);
    const ImU32 minor_color = IM_COL32(44, 62, 82, 112);

    auto draw_line_segment = [&](const Vec3& a, const Vec3& b, ImU32 color, float thickness) {
        DrawLine3D(draw_list, a, b, color, thickness, canvas_pos, canvas_size);
    };

    const float start = std::floor((center.x - half_extent) / step) * step;
    const float end = std::ceil((center.x + half_extent) / step) * step;

    if (active_sketch_plane_ == SketchPlane_XY) {
        for (float x = start; x <= end; x += step) {
            const bool is_major = std::fmod(std::fabs(x - center.x) / step, 5.0f) < 0.01f;
            draw_line_segment({x, center.y - half_extent, 0.0f}, {x, center.y + half_extent, 0.0f}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
        for (float y = start; y <= end; y += step) {
            const bool is_major = std::fmod(std::fabs(y - center.y) / step, 5.0f) < 0.01f;
            draw_line_segment({center.x - half_extent, y, 0.0f}, {center.x + half_extent, y, 0.0f}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
    } else if (active_sketch_plane_ == SketchPlane_YZ) {
        for (float y = start; y <= end; y += step) {
            const bool is_major = std::fmod(std::fabs(y - center.y) / step, 5.0f) < 0.01f;
            draw_line_segment({0.0f, y, center.z - half_extent}, {0.0f, y, center.z + half_extent}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
        for (float z = start; z <= end; z += step) {
            const bool is_major = std::fmod(std::fabs(z - center.z) / step, 5.0f) < 0.01f;
            draw_line_segment({0.0f, center.y - half_extent, z}, {0.0f, center.y + half_extent, z}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
    } else if (active_sketch_plane_ == SketchPlane_XZ) {
        for (float x = start; x <= end; x += step) {
            const bool is_major = std::fmod(std::fabs(x - center.x) / step, 5.0f) < 0.01f;
            draw_line_segment({x, 0.0f, center.z - half_extent}, {x, 0.0f, center.z + half_extent}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
        for (float z = start; z <= end; z += step) {
            const bool is_major = std::fmod(std::fabs(z - center.z) / step, 5.0f) < 0.01f;
            draw_line_segment({center.x - half_extent, 0.0f, z}, {center.x + half_extent, 0.0f, z}, is_major ? major_color : minor_color, is_major ? 1.3f : 1.0f);
        }
    }
}

bool ViewportWindow::GetCanvasRect(ImVec2* out_pos, ImVec2* out_size) const {
    if (out_pos == nullptr || out_size == nullptr) {
        return false;
    }
    *out_pos = last_canvas_pos_;
    *out_size = last_canvas_size_;
    return true;
}

ViewportWindow::Vec3 ViewportWindow::CameraPosition() const {
    const float cp = std::cos(pitch_);
    const float sp = std::sin(pitch_);
    const float cy = std::cos(yaw_);
    const float sy = std::sin(yaw_);
    const Vec3 offset = {distance_ * cp * cy, distance_ * cp * sy, distance_ * sp};
    return Add(target_, offset);
}

ViewportWindow::Vec3 ViewportWindow::CameraForward() const {
    return Normalize(Sub(target_, CameraPosition()));
}

ViewportWindow::Vec3 ViewportWindow::CameraRight() const {
    const Vec3 forward = CameraForward();
    Vec3 right = Cross(forward, {0.0f, 0.0f, 1.0f});
    if (Dot(right, right) < 1e-8f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    return Normalize(right);
}

ViewportWindow::Vec3 ViewportWindow::CameraUp() const {
    return Normalize(Cross(CameraRight(), CameraForward()));
}

bool ViewportWindow::ProjectToScreen(const Vec3& world, const ImVec2& canvas_pos, const ImVec2& canvas_size, ImVec2& screen) const {
    const Vec3 cam_pos = CameraPosition();
    const Vec3 cam_forward = CameraForward();
    const Vec3 cam_right = CameraRight();
    const Vec3 cam_up = CameraUp();

    const Vec3 rel = Sub(world, cam_pos);
    const float z = Dot(rel, cam_forward);
    if (z <= 0.05f) {
        return false;
    }

    const float x = Dot(rel, cam_right);
    const float y = Dot(rel, cam_up);

    const ImVec2 center(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);

    if (ortho_mode_) {
        const float ortho_scale = canvas_size.y / (distance_ * 1.8f);
        screen.x = center.x + (x * ortho_scale);
        screen.y = center.y - (y * ortho_scale);
        return true;
    }

    const float fov_rad = 55.0f * (Pi / 180.0f);
    const float focal = (canvas_size.y * 0.5f) / std::tan(fov_rad * 0.5f);
    float projected_z = z;
    if (persp_w_ortho_faces_mode_) {
        projected_z = z * 0.35f + distance_ * 0.65f;
        if (projected_z <= 0.05f) {
            projected_z = 0.05f;
        }
    }

    screen.x = center.x + (x * focal / projected_z);
    screen.y = center.y - (y * focal / projected_z);
    return true;
}

void ViewportWindow::DrawLine3D(
    ImDrawList* draw_list,
    const Vec3& a,
    const Vec3& b,
    ImU32 color,
    float thickness,
    const ImVec2& canvas_pos,
    const ImVec2& canvas_size
) const {
    const Vec3 cam_pos = CameraPosition();
    const Vec3 cam_forward = CameraForward();
    const Vec3 cam_right = CameraRight();
    const Vec3 cam_up = CameraUp();

    const Vec3 rel_a = Sub(a, cam_pos);
    const Vec3 rel_b = Sub(b, cam_pos);

    float ax = Dot(rel_a, cam_right);
    float ay = Dot(rel_a, cam_up);
    float az = Dot(rel_a, cam_forward);
    float bx = Dot(rel_b, cam_right);
    float by = Dot(rel_b, cam_up);
    float bz = Dot(rel_b, cam_forward);

    const float near_z = 0.01f;
    if (az <= near_z && bz <= near_z) {
        return;
    }

    if (az <= near_z || bz <= near_z) {
        const float dz = bz - az;
        if (std::fabs(dz) < 1e-6f) {
            return;
        }
        const float t = (near_z - az) / dz;
        const float cx = ax + (bx - ax) * t;
        const float cy = ay + (by - ay) * t;
        const float cz = near_z;

        if (az <= near_z) {
            ax = cx;
            ay = cy;
            az = cz;
        } else {
            bx = cx;
            by = cy;
            bz = cz;
        }
    }

    const ImVec2 center(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);

    ImVec2 sa;
    ImVec2 sb;
    if (ortho_mode_) {
        const float ortho_scale = canvas_size.y / (distance_ * 1.8f);
        sa = ImVec2(center.x + (ax * ortho_scale), center.y - (ay * ortho_scale));
        sb = ImVec2(center.x + (bx * ortho_scale), center.y - (by * ortho_scale));
    } else {
        const float fov_rad = 55.0f * (Pi / 180.0f);
        const float focal = (canvas_size.y * 0.5f) / std::tan(fov_rad * 0.5f);
        float az_proj = az;
        float bz_proj = bz;
        if (persp_w_ortho_faces_mode_) {
            az_proj = az * 0.35f + distance_ * 0.65f;
            bz_proj = bz * 0.35f + distance_ * 0.65f;
            if (az_proj <= 0.05f) {
                az_proj = 0.05f;
            }
            if (bz_proj <= 0.05f) {
                bz_proj = 0.05f;
            }
        }
        sa = ImVec2(center.x + (ax * focal / az_proj), center.y - (ay * focal / az_proj));
        sb = ImVec2(center.x + (bx * focal / bz_proj), center.y - (by * focal / bz_proj));
    }
    draw_list->AddLine(sa, sb, color, thickness);
}

void ViewportWindow::DrawCircle3D(
    ImDrawList* draw_list,
    const Vec3& center,
    float radius,
    ImU32 color,
    float thickness,
    const ImVec2& canvas_pos,
    const ImVec2& canvas_size
) const {
    if (draw_list == nullptr || radius <= 0.0f) {
        return;
    }

    Vec3 axis_u = {1.0f, 0.0f, 0.0f};
    Vec3 axis_v = {0.0f, 1.0f, 0.0f};
    switch (active_sketch_plane_) {
        case SketchPlane_XY:
            axis_u = {1.0f, 0.0f, 0.0f};
            axis_v = {0.0f, 1.0f, 0.0f};
            break;
        case SketchPlane_YZ:
            axis_u = {0.0f, 1.0f, 0.0f};
            axis_v = {0.0f, 0.0f, 1.0f};
            break;
        case SketchPlane_XZ:
            axis_u = {1.0f, 0.0f, 0.0f};
            axis_v = {0.0f, 0.0f, 1.0f};
            break;
        case SketchPlane_None:
        default:
            return;
    }

    const int segments = 48;
    Vec3 previous = Add(Add(center, Mul(axis_u, radius)), Mul(axis_v, 0.0f));
    for (int i = 1; i <= segments; ++i) {
        const float t = (float)i / (float)segments;
        const float angle = t * 6.2831853f;
        const Vec3 point = Add(center, Add(Mul(axis_u, std::cos(angle) * radius), Mul(axis_v, std::sin(angle) * radius)));
        DrawLine3D(draw_list, previous, point, color, thickness, canvas_pos, canvas_size);
        previous = point;
    }
}

void ViewportWindow::DrawPlaneQuad3D(
    ImDrawList* draw_list,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d,
    ImU32 fill_color,
    ImU32 outline_color,
    float outline_thickness,
    const ImVec2& canvas_pos,
    const ImVec2& canvas_size
) const {
    ImVec2 pa;
    ImVec2 pb;
    ImVec2 pc;
    ImVec2 pd;
    if (!ProjectToScreen(a, canvas_pos, canvas_size, pa) ||
        !ProjectToScreen(b, canvas_pos, canvas_size, pb) ||
        !ProjectToScreen(c, canvas_pos, canvas_size, pc) ||
        !ProjectToScreen(d, canvas_pos, canvas_size, pd)) {
        return;
    }

    ImVec2 pts[4] = {pa, pb, pc, pd};
    draw_list->AddConvexPolyFilled(pts, 4, fill_color);
    draw_list->AddPolyline(pts, 4, outline_color, ImDrawFlags_Closed, outline_thickness);
}

bool ViewportWindow::PointInTriangle2D(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c) const {
    const float v0x = c.x - a.x;
    const float v0y = c.y - a.y;
    const float v1x = b.x - a.x;
    const float v1y = b.y - a.y;
    const float v2x = p.x - a.x;
    const float v2y = p.y - a.y;

    const float dot00 = v0x * v0x + v0y * v0y;
    const float dot01 = v0x * v1x + v0y * v1y;
    const float dot02 = v0x * v2x + v0y * v2y;
    const float dot11 = v1x * v1x + v1y * v1y;
    const float dot12 = v1x * v2x + v1y * v2y;

    const float denom = dot00 * dot11 - dot01 * dot01;
    if (std::fabs(denom) < 1e-8f) {
        return false;
    }
    const float inv_denom = 1.0f / denom;
    const float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
    const float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
    return (u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f);
}

bool ViewportWindow::PointInQuad2D(const ImVec2& p, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d) const {
    return PointInTriangle2D(p, a, b, c) || PointInTriangle2D(p, a, c, d);
}

bool ViewportWindow::ProjectQuadToScreen(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    const Vec3& d,
    const ImVec2& canvas_pos,
    const ImVec2& canvas_size,
    ImVec2& sa,
    ImVec2& sb,
    ImVec2& sc,
    ImVec2& sd
) const {
    return ProjectToScreen(a, canvas_pos, canvas_size, sa) &&
           ProjectToScreen(b, canvas_pos, canvas_size, sb) &&
           ProjectToScreen(c, canvas_pos, canvas_size, sc) &&
           ProjectToScreen(d, canvas_pos, canvas_size, sd);
}

void ViewportWindow::AlignCameraToSketchPlane(SketchPlane plane) {
    switch (plane) {
        case SketchPlane_XY:
            yaw_ = 0.0f;
            pitch_ = kHalfPi;
            break;
        case SketchPlane_YZ:
            yaw_ = 0.0f;
            pitch_ = 0.0f;
            break;
        case SketchPlane_XZ:
            yaw_ = 1.5707963f;
            pitch_ = 0.0f;
            break;
        case SketchPlane_None:
        default:
            return;
    }

    SetSolidMode(false);

    ortho_mode_ = true;
    persp_mode_ = false;
    persp_w_ortho_faces_mode_ = false;
}

void ViewportWindow::Render(const ImGuiIO& io) {
    const double info_hold_seconds = 3.0;
    const double info_fade_seconds = 0.75;

    ImGuiWindowFlags viewport_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Viewport", nullptr, viewport_flags);

    // Keep the canvas separated from both the host top bar and the viewport header/tab area.
    const float ui_scale = (io.FontGlobalScale > 0.0f) ? io.FontGlobalScale : 1.0f;
    const float viewport_header_allowance = ImGui::GetFrameHeight() * 0.35f;
    const float host_bar_allowance = 4.0f * ui_scale;
    float canvas_top_gap = viewport_header_allowance + host_bar_allowance;
    if (canvas_top_gap < 8.0f) {
        canvas_top_gap = 8.0f;
    }
    ImGui::Dummy(ImVec2(0.0f, canvas_top_gap));

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    last_canvas_pos_ = canvas_pos;
    last_canvas_size_ = canvas_size;
    if (canvas_size.x < 64.0f) {
        canvas_size.x = 64.0f;
    }
    if (canvas_size.y < 64.0f) {
        canvas_size.y = 64.0f;
    }

    ImGuiButtonFlags viewport_button_flags = ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight;
    ImGui::InvisibleButton("viewport_canvas", canvas_size, viewport_button_flags);
    const bool hovered = ImGui::IsItemHovered();
    if (hovered) {
        if (io.MouseWheel != 0.0f) {
            distance_ *= std::exp(-io.MouseWheel * 0.12f);
            if (distance_ < 2.0f) {
                distance_ = 2.0f;
            }
            if (distance_ > 300.0f) {
                distance_ = 300.0f;
            }
            info_last_scroll_time_ = ImGui::GetTime();
        }

        if (ImGui::IsMouseDragging(drag_button_)) {
            yaw_ -= io.MouseDelta.x * 0.01f;
            pitch_ += io.MouseDelta.y * 0.01f;
            if (pitch_ > kHalfPi) {
                pitch_ = kHalfPi;
            }
            if (pitch_ < -kHalfPi) {
                pitch_ = -kHalfPi;
            }
            info_last_scroll_time_ = ImGui::GetTime();
        }

        if (ImGui::IsMouseDragging(orbit_button_)) {
            const float pan_speed = 0.0035f * distance_;
            const Vec3 right = CameraRight();
            const Vec3 up = CameraUp();
            target_ = Add(target_, Add(Mul(right, -io.MouseDelta.x * pan_speed), Mul(up, io.MouseDelta.y * pan_speed)));
            info_last_scroll_time_ = ImGui::GetTime();
        }
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_end(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);
    draw_list->PushClipRect(canvas_pos, canvas_end, true);
    draw_list->AddRectFilled(canvas_pos, canvas_end, IM_COL32(20, 24, 32, 255));

    if (show_layout_grid_) {
        const int half_lines = 24;
        for (int i = -half_lines; i <= half_lines; ++i) {
            const float p = (float)i;
            const ImU32 color = (i % 5 == 0) ? IM_COL32(68, 74, 86, 255) : IM_COL32(46, 52, 64, 255);
            DrawLine3D(
                draw_list,
                {-half_lines, p, 0.0f},
                {half_lines, p, 0.0f},
                color,
                1.0f,
                canvas_pos,
                canvas_size
            );
            DrawLine3D(
                draw_list,
                {p, -half_lines, 0.0f},
                {p, half_lines, 0.0f},
                color,
                1.0f,
                canvas_pos,
                canvas_size
            );
        }
    }

    const float plane_extent = 8.0f;
    const float plane_padding = 0.8f;
    const float plane_min = plane_padding;
    const float plane_max = plane_extent - plane_padding;

    const Vec3 xy_a = {plane_min, plane_min, 0.0f};
    const Vec3 xy_b = {plane_max, plane_min, 0.0f};
    const Vec3 xy_c = {plane_max, plane_max, 0.0f};
    const Vec3 xy_d = {plane_min, plane_max, 0.0f};

    const Vec3 yz_a = {0.0f, plane_min, plane_min};
    const Vec3 yz_b = {0.0f, plane_max, plane_min};
    const Vec3 yz_c = {0.0f, plane_max, plane_max};
    const Vec3 yz_d = {0.0f, plane_min, plane_max};

    const Vec3 xz_a = {plane_min, 0.0f, plane_min};
    const Vec3 xz_b = {plane_max, 0.0f, plane_min};
    const Vec3 xz_c = {plane_max, 0.0f, plane_max};
    const Vec3 xz_d = {plane_min, 0.0f, plane_max};

    struct HoverPlane {
        SketchPlane plane = SketchPlane_None;
        ImVec2 a;
        ImVec2 b;
        ImVec2 c;
        ImVec2 d;
        float depth = 0.0f;
        bool valid = false;
    };

    auto make_hover_plane = [&](SketchPlane plane, const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pd) {
        HoverPlane hp;
        hp.plane = plane;
        hp.valid = ProjectQuadToScreen(pa, pb, pc, pd, canvas_pos, canvas_size, hp.a, hp.b, hp.c, hp.d);
        const Vec3 center = Mul(Add(Add(pa, pb), Add(pc, pd)), 0.25f);
        hp.depth = Dot(Sub(center, CameraPosition()), CameraForward());
        return hp;
    };

    HoverPlane hover_planes[3] = {
        make_hover_plane(SketchPlane_XY, xy_a, xy_b, xy_c, xy_d),
        make_hover_plane(SketchPlane_YZ, yz_a, yz_b, yz_c, yz_d),
        make_hover_plane(SketchPlane_XZ, xz_a, xz_b, xz_c, xz_d),
    };

    SketchPlane hovered_plane = SketchPlane_None;
    float hovered_depth = -1e30f;
    if (awaiting_sketch_plane_selection_ && hovered) {
        const ImVec2 mouse_pos = io.MousePos;
        for (const HoverPlane& hp : hover_planes) {
            if (!hp.valid) {
                continue;
            }
            if (!PointInQuad2D(mouse_pos, hp.a, hp.b, hp.c, hp.d)) {
                continue;
            }
            if (hp.depth > hovered_depth) {
                hovered_depth = hp.depth;
                hovered_plane = hp.plane;
            }
        }
    }

    auto plane_fill_color = [&](SketchPlane plane) -> ImU32 {
        if (awaiting_sketch_plane_selection_ && hovered_plane == plane) {
            return IM_COL32(255, 156, 72, 78);
        }
        return IM_COL32(255, 146, 56, 46);
    };

    auto plane_outline_color = [&](SketchPlane plane) -> ImU32 {
        if (awaiting_sketch_plane_selection_ && hovered_plane == plane) {
            return IM_COL32(255, 184, 112, 210);
        }
        return IM_COL32(255, 166, 84, 132);
    };

    const bool show_sketch_planes = awaiting_sketch_plane_selection_ || !sketch_mode_active_;
    if (show_sketch_planes) {
        DrawPlaneQuad3D(
            draw_list,
            xy_a,
            xy_b,
            xy_c,
            xy_d,
            plane_fill_color(SketchPlane_XY),
            plane_outline_color(SketchPlane_XY),
            1.0f,
            canvas_pos,
            canvas_size
        );
        DrawPlaneQuad3D(
            draw_list,
            yz_a,
            yz_b,
            yz_c,
            yz_d,
            plane_fill_color(SketchPlane_YZ),
            plane_outline_color(SketchPlane_YZ),
            1.0f,
            canvas_pos,
            canvas_size
        );
        DrawPlaneQuad3D(
            draw_list,
            xz_a,
            xz_b,
            xz_c,
            xz_d,
            plane_fill_color(SketchPlane_XZ),
            plane_outline_color(SketchPlane_XZ),
            1.0f,
            canvas_pos,
            canvas_size
        );
    }

    if (sketch_mode_active_ && active_sketch_plane_ != SketchPlane_None) {
        DrawAdaptiveSketchGrid(draw_list, canvas_pos, canvas_size);
    }

    // Draw committed line segments in both sketch and solid mode.
    const bool has_committed_geometry = !committed_line_segments_.empty();
    if (has_committed_geometry) {
        const ImU32 committed_color = IM_COL32(150, 200, 100, 255); // Green for committed lines
        for (const auto& segment : committed_line_segments_) {
            DrawLine3D(draw_list, segment.start, segment.end, committed_color, 2.0f, canvas_pos, canvas_size);
        }
    }

    if (has_committed_geometry) {
        // TODO: This detection is kinda broken and dosent handle multiple polygons that well.
        // Detect closed polygons from committed segments and render a subtle fill
        const ImU32 polygon_fill_default = IM_COL32(255, 255, 255, 0); // subtle lightening
        const ImU32 polygon_fill_hover = IM_COL32(255, 255, 255, 80); // slightly lighter on hover
        const ImU32 polygon_fill_selected = IM_COL32(255, 255, 255, 80); // 2x lighter on click
        const ImU32 overlap_fill_default = IM_COL32(255, 255, 255, 0);
        const ImU32 overlap_fill_hover = IM_COL32(255, 255, 255, 80);
        const ImU32 overlap_fill_selected = IM_COL32(255, 255, 255, 80);
        const ImU32 polygon_outline = IM_COL32(200, 220, 180, 160);
        
        // Group segments by group ID and detect polygons per group
        std::map<int, std::vector<std::pair<ImVec2,ImVec2>>> grouped_segs;
        for (size_t i = 0; i < committed_line_segments_.size(); ++i) {
            const auto &s = committed_line_segments_[i];
            int group_id = (i < segment_group_ids_.size()) ? segment_group_ids_[i] : -1;
            
            ImVec2 a, b;
            if (!ProjectToScreen(s.start, canvas_pos, canvas_size, a)) continue;
            if (!ProjectToScreen(s.end, canvas_pos, canvas_size, b)) continue;
            
            if (group_id == -1 || grouped_segs.find(group_id) == grouped_segs.end()) {
                grouped_segs[group_id] = std::vector<std::pair<ImVec2,ImVec2>>();
            }
            grouped_segs[group_id].emplace_back(a, b);
        }

        std::vector<std::vector<ImVec2>> polys2;
        std::vector<int> poly_group_ids;
        
        for (const auto& [group_id, segs] : grouped_segs) {
            auto group_polys = BuildPolygonsFromSegments2D(segs);
            for (const auto& poly : group_polys) {
                polys2.push_back(poly);
                poly_group_ids.push_back(group_id);
            }
        }

        struct OverlapRegion {
            std::vector<ImVec2> pts;
            std::vector<int> polygon_indices;
        };

        std::vector<OverlapRegion> overlap_regions;
        const int P = (int)polys2.size();
        if (P >= 2) {
            // Enumerate all non-empty subsets of polygons with size >= 2
            const int maxmask = 1 << P;
            for (int mask = 0; mask < maxmask; ++mask) {
                // count bits
                int pc = 0;
                for (int t = mask; t; t &= (t - 1)) ++pc;
                if (pc < 2) continue;

                // collect indices
                std::vector<int> idxs;
                idxs.reserve(pc);
                for (int i = 0; i < P; ++i) if (mask & (1 << i)) idxs.push_back(i);

                // Start with the first polygon as subject and clip by the others (require clips to be convex)
                std::vector<ImVec2> subject = polys2[idxs[0]];
                if (subject.size() < 3) continue;
                bool ok = true;
                for (size_t k = 1; k < idxs.size(); ++k) {
                    const auto& clip_poly = polys2[idxs[k]];
                    if (!IsConvexPolygon2D(clip_poly)) { ok = false; break; }
                    subject = ClipPolygonAgainstConvexClip2D(subject, clip_poly);
                    if (subject.size() < 3) { ok = false; break; }
                }
                if (!ok) continue;

                const float area = std::fabs(PolygonSignedArea2D(subject));
                if (area < 8.0f) continue;

                overlap_regions.push_back({subject, idxs});
            }
        }
        
        // Test for hover on each polygon
        hovered_polygon_index_ = -1;
        hovered_overlap_index_ = -1;
        if (hovered) {
            const ImVec2 mouse_pos = io.MousePos;

            for (int overlap_idx = 0; overlap_idx < (int)overlap_regions.size(); ++overlap_idx) {
                const auto& pts = overlap_regions[overlap_idx].pts;
                if (pts.size() < 3) {
                    continue;
                }

                bool inside = false;
                for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
                    const ImVec2& a = pts[i];
                    const ImVec2& b = pts[j];
                    const bool intersects = ((a.y > mouse_pos.y) != (b.y > mouse_pos.y)) &&
                        (mouse_pos.x < (b.x - a.x) * (mouse_pos.y - a.y) / (b.y - a.y) + a.x);
                    if (intersects) {
                        inside = !inside;
                    }
                }

                if (inside) {
                    hovered_overlap_index_ = overlap_idx;
                    break;
                }
            }

            for (int poly_idx = 0; poly_idx < (int)polys2.size(); ++poly_idx) {
                const auto& pts = polys2[poly_idx];
                if (pts.size() < 3) continue;
                
                // Point-in-polygon test using ray casting
                bool inside = false;
                for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
                    const ImVec2& a = pts[i];
                    const ImVec2& b = pts[j];
                    const bool intersects = ((a.y > mouse_pos.y) != (b.y > mouse_pos.y)) &&
                        (mouse_pos.x < (b.x - a.x) * (mouse_pos.y - a.y) / (b.y - a.y) + a.x);
                    if (intersects) inside = !inside;
                }
                
                if (inside) {
                    if (hovered_overlap_index_ == -1) {
                        hovered_polygon_index_ = poly_idx;
                    }
                    break; // Only track the topmost hovered polygon
                }
            }
            
            // Handle click on hovered polygon
            if (hovered_overlap_index_ != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_overlap_index_ = hovered_overlap_index_;
                selected_polygon_index_ = -1;
                selected_polygon_points_ = overlap_regions[hovered_overlap_index_].pts;
                selected_polygon_world_points_.clear();
                for (const ImVec2& pt : selected_polygon_points_) {
                    selected_polygon_world_points_.push_back(ScreenToWorldPlane(pt, canvas_pos, canvas_size, sketch_geometry_plane_));
                }
            } else if (hovered_polygon_index_ != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_polygon_index_ = hovered_polygon_index_;
                selected_overlap_index_ = -1;
                selected_polygon_points_ = polys2[hovered_polygon_index_];
                selected_polygon_world_points_.clear();
                for (const ImVec2& pt : selected_polygon_points_) {
                    selected_polygon_world_points_.push_back(ScreenToWorldPlane(pt, canvas_pos, canvas_size, sketch_geometry_plane_));
                }
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                selected_polygon_points_.clear();
                selected_polygon_world_points_.clear();
            }
        }
        
        struct FillSpan {
            int y = 0;
            float x0 = 0.0f;
            float x1 = 0.0f;
            int polygon_index = -1;
        };

        struct OverlapSpan {
            int y = 0;
            float x0 = 0.0f;
            float x1 = 0.0f;
            int overlap_idx = -1;
        };

        std::vector<FillSpan> fill_spans;
        std::vector<OverlapSpan> overlap_spans;
        float min_y = 1e30f;
        float max_y = -1e30f;
        for (const auto& pts2 : polys2) {
            for (const ImVec2& p : pts2) {
                min_y = (std::min)(min_y, p.y);
                max_y = (std::max)(max_y, p.y);
            }
        }

        if (min_y < max_y) {
            const int y_start = (int)std::floor(min_y);
            const int y_end = (int)std::ceil(max_y);

            for (int y = y_start; y < y_end; ++y) {
                const float sample_y = (float)y + 0.5f;
                std::vector<std::tuple<float, float, int>> row_intervals; // x0, x1, poly_idx

                for (int poly_idx = 0; poly_idx < (int)polys2.size(); ++poly_idx) {
                    const auto& pts2 = polys2[poly_idx];
                    if (pts2.size() < 3) {
                        continue;
                    }

                    std::vector<float> intersections;
                    intersections.reserve(pts2.size());

                    for (size_t i = 0; i < pts2.size(); ++i) {
                        const ImVec2& a = pts2[i];
                        const ImVec2& b = pts2[(i + 1) % pts2.size()];
                        const bool crosses = (a.y <= sample_y && b.y > sample_y) || (b.y <= sample_y && a.y > sample_y);
                        if (!crosses) {
                            continue;
                        }

                        const float t = (sample_y - a.y) / (b.y - a.y);
                        intersections.push_back(a.x + t * (b.x - a.x));
                    }

                    if (intersections.size() < 2) {
                        continue;
                    }

                    std::sort(intersections.begin(), intersections.end());
                    for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                        const float x0 = intersections[i];
                        const float x1 = intersections[i + 1];
                        if (x1 > x0) {
                            row_intervals.emplace_back(x0, x1, poly_idx);
                        }
                    }
                }

                if (row_intervals.empty()) {
                    continue;
                }

                std::sort(row_intervals.begin(), row_intervals.end(), [](const auto& lhs, const auto& rhs) {
                    if (std::get<0>(lhs) != std::get<0>(rhs)) {
                        return std::get<0>(lhs) < std::get<0>(rhs);
                    }
                    return std::get<1>(lhs) < std::get<1>(rhs);
                });

                // For each interval from each polygon, create a fill span directly
                // Don't merge intervals from different polygons
                for (const auto& interval : row_intervals) {
                    const float x0 = std::get<0>(interval);
                    const float x1 = std::get<1>(interval);
                    const int poly_idx = std::get<2>(interval);
                    fill_spans.push_back({y, x0, x1, poly_idx});
                }
            }
        }

        if (min_y < max_y) {
            const int y_start = (int)std::floor(min_y);
            const int y_end = (int)std::ceil(max_y);

            for (int overlap_idx = 0; overlap_idx < (int)overlap_regions.size(); ++overlap_idx) {
                const auto& pts2 = overlap_regions[overlap_idx].pts;
                if (pts2.size() < 3) {
                    continue;
                }

                for (int y = y_start; y < y_end; ++y) {
                    const float sample_y = (float)y + 0.5f;
                    std::vector<float> intersections;
                    intersections.reserve(pts2.size());

                    for (size_t i = 0; i < pts2.size(); ++i) {
                        const ImVec2& a = pts2[i];
                        const ImVec2& b = pts2[(i + 1) % pts2.size()];
                        const bool crosses = (a.y <= sample_y && b.y > sample_y) || (b.y <= sample_y && a.y > sample_y);
                        if (!crosses) continue;

                        const float t = (sample_y - a.y) / (b.y - a.y);
                        intersections.push_back(a.x + t * (b.x - a.x));
                    }

                    if (intersections.size() < 2) continue;

                    std::sort(intersections.begin(), intersections.end());
                    for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
                        const float x0 = intersections[i];
                        const float x1 = intersections[i + 1];
                        if (x1 > x0) {
                            overlap_spans.push_back({y, x0, x1, overlap_idx});
                        }
                    }
                }
            }
        }

        for (const FillSpan& span : fill_spans) {
            std::vector<std::pair<float, float>> cutouts;
            for (const auto& overlap : overlap_spans) {
                if (overlap.y != span.y) continue;
                const int oi = overlap.overlap_idx;
                if (oi < 0 || oi >= (int)overlap_regions.size()) continue;
                const auto &idxs = overlap_regions[oi].polygon_indices;
                if (std::find(idxs.begin(), idxs.end(), span.polygon_index) == idxs.end()) continue;
                cutouts.emplace_back(overlap.x0, overlap.x1);
            }

            std::sort(cutouts.begin(), cutouts.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.first != rhs.first) {
                    return lhs.first < rhs.first;
                }
                return lhs.second < rhs.second;
            });

            std::vector<std::pair<float, float>> visible_segments;
            float cursor = span.x0;
            for (const auto& cutout : cutouts) {
                const float cut_x0 = (std::max)(cutout.first, span.x0);
                const float cut_x1 = (std::min)(cutout.second, span.x1);
                if (cut_x1 <= cut_x0) {
                    continue;
                }

                if (cut_x0 > cursor) {
                    visible_segments.emplace_back(cursor, cut_x0);
                }
                cursor = (std::max)(cursor, cut_x1);
            }

            if (cursor < span.x1) {
                visible_segments.emplace_back(cursor, span.x1);
            }

            if (visible_segments.empty()) {
                continue;
            }

            ImU32 fill_color = polygon_fill_default;
            if (span.polygon_index == selected_polygon_index_) {
                fill_color = polygon_fill_selected;
            } else if (span.polygon_index == hovered_polygon_index_) {
                fill_color = polygon_fill_hover;
            }

            for (const auto& visible : visible_segments) {
                draw_list->AddRectFilled(
                    ImVec2(visible.first, (float)span.y),
                    ImVec2(visible.second, (float)span.y + 1.0f),
                    fill_color);
            }
        }

        // Render overlap regions using the same scanline grid as base fills to avoid seams
        std::vector<ImU32> overlap_colors(overlap_regions.size(), overlap_fill_default);
        for (int overlap_idx = 0; overlap_idx < (int)overlap_regions.size(); ++overlap_idx) {
            if (overlap_idx == selected_overlap_index_) overlap_colors[overlap_idx] = overlap_fill_selected;
            else if (overlap_idx == hovered_overlap_index_) overlap_colors[overlap_idx] = overlap_fill_hover;
        }

        for (const auto& ospan : overlap_spans) {
            const int oi = ospan.overlap_idx;
            if (oi < 0 || oi >= (int)overlap_regions.size()) continue;
            ImU32 fill_color = overlap_colors[oi];
            draw_list->AddRectFilled(
                ImVec2(ospan.x0, (float)ospan.y),
                ImVec2(ospan.x1, (float)ospan.y + 1.0f),
                fill_color);
        }

        // Draw outlines for overlap regions (using the polygon outlines)
        for (int overlap_idx = 0; overlap_idx < (int)overlap_regions.size(); ++overlap_idx) {
            const auto& overlap = overlap_regions[overlap_idx];
            if (overlap.pts.size() < 3) continue;
            draw_list->AddPolyline(overlap.pts.data(), (int)overlap.pts.size(), polygon_outline, ImDrawFlags_Closed, 2.0f);
        }

        for (const auto &pts2 : polys2) {
            if (pts2.size() < 3) continue;
            draw_list->AddPolyline(pts2.data(), (int)pts2.size(), polygon_outline, ImDrawFlags_Closed, 1.5f);
        }

        if (extruded_body_final_visible_ && extruded_body_final_polygon_.size() >= 3) {
            const Vec3 cam_pos = CameraPosition();
            const Vec3 cam_forward = CameraForward();
            DrawExtrudedBodyPreview(draw_list, extruded_body_final_polygon_, extruded_body_final_depth_world_, this, canvas_pos, canvas_size, true, cam_pos, cam_forward);
        }

        if (extruded_body_preview_visible_ && extruded_body_preview_polygon_.size() >= 3) {
            const Vec3 cam_pos = CameraPosition();
            const Vec3 cam_forward = CameraForward();
            DrawExtrudedBodyPreview(draw_list, extruded_body_preview_polygon_, extruded_body_preview_depth_world_, this, canvas_pos, canvas_size, false, cam_pos, cam_forward);
        }
    }

    if (awaiting_sketch_plane_selection_ && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        struct HitPlane {
            SketchPlane plane;
            ImVec2 a;
            ImVec2 b;
            ImVec2 c;
            ImVec2 d;
            float depth;
            bool valid;
        };

        const ImVec2 mouse_pos = io.MousePos;

        auto make_hit_plane = [&](SketchPlane plane, const Vec3& pa, const Vec3& pb, const Vec3& pc, const Vec3& pd) {
            HitPlane hp = {};
            hp.plane = plane;
            hp.valid = ProjectQuadToScreen(pa, pb, pc, pd, canvas_pos, canvas_size, hp.a, hp.b, hp.c, hp.d);
            const Vec3 center = Mul(Add(Add(pa, pb), Add(pc, pd)), 0.25f);
            hp.depth = Dot(Sub(center, CameraPosition()), CameraForward());
            return hp;
        };

        HitPlane hit_planes[3] = {
            make_hit_plane(SketchPlane_XY, xy_a, xy_b, xy_c, xy_d),
            make_hit_plane(SketchPlane_YZ, yz_a, yz_b, yz_c, yz_d),
            make_hit_plane(SketchPlane_XZ, xz_a, xz_b, xz_c, xz_d),
        };

        SketchPlane best_plane = SketchPlane_None;
        float best_depth = -1e30f;
        for (const HitPlane& hp : hit_planes) {
            if (!hp.valid) {
                continue;
            }
            if (!PointInQuad2D(mouse_pos, hp.a, hp.b, hp.c, hp.d)) {
                continue;
            }
            if (hp.depth > best_depth) {
                best_depth = hp.depth;
                best_plane = hp.plane;
            }
        }

        if (best_plane != SketchPlane_None) {
            selected_sketch_plane_ = best_plane;
            awaiting_sketch_plane_selection_ = false;
            AlignCameraToSketchPlane(best_plane);
        }
    }

    // Sketch drawing interaction
    if (sketch_mode_active_ && hovered) {
        const ImVec2 mouse_screen = io.MousePos;
        const SnapTarget snap_target = ResolveSnapTarget(mouse_screen, canvas_pos, canvas_size);
        const Vec3 snapped_pos = snap_target.world;
        const bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool started_new_shape = false;

        // Draw cursor crosshair at snapped position
        ImVec2 crosshair_screen;
        if (snap_target.valid && ProjectToScreen(snap_target.world, canvas_pos, canvas_size, crosshair_screen) && crosshair_enabled_) {
            const ImU32 crosshair_color = IM_COL32(100, 200, 255, 220);
            // Draw a square only when snapping to a geometry vertex (endpoint).
            // Do NOT show the square when gliding on a segment interior, insteadshow
            // the regular crosshair in that case to avoid misleading the user.
            const bool is_endpoint_snap = snap_target.snapped_to_geometry && !snap_target.snapped_to_segment_interior;
            if (is_endpoint_snap) {
                const float half_size = 6.0f;
                draw_list->AddRect(
                    ImVec2(crosshair_screen.x - half_size, crosshair_screen.y - half_size),
                    ImVec2(crosshair_screen.x + half_size, crosshair_screen.y + half_size),
                    crosshair_color,
                    0.0f,
                    0,
                    1.8f
                );
            } else {
                const float crosshair_size = 6.0f;
                draw_list->AddLine(
                    ImVec2(crosshair_screen.x - crosshair_size, crosshair_screen.y),
                    ImVec2(crosshair_screen.x + crosshair_size, crosshair_screen.y),
                    crosshair_color,
                    1.5f
                );
                draw_list->AddLine(
                    ImVec2(crosshair_screen.x, crosshair_screen.y - crosshair_size),
                    ImVec2(crosshair_screen.x, crosshair_screen.y + crosshair_size),
                    crosshair_color,
                    1.5f
                );
                draw_list->AddCircle(crosshair_screen, 2.5f, crosshair_color, 6, 1.2f);
            }
        }

        if (active_sketch_tool_ == SketchToolMode::Line) {
            // Handle line start point click (only if not already drawing)
            if (!is_drawing_line_ && awaiting_line_start_ && left_clicked) {
                line_start_ = snap_target.world;
                line_end_ = snap_target.world;
                line_start_snap_ = snap_target;
                line_end_snap_ = snap_target;
                current_line_group_id_ = next_segment_group_++;
                is_drawing_line_ = true;
                awaiting_line_start_ = false;
                focused_line_field_ = 1; // Default to length field
                started_new_shape = true;
            }

            // Handle second click to finish line and start new one
            if (is_drawing_line_ && left_clicked && !started_new_shape) {
                line_end_ = snap_target.world;
                line_end_snap_ = snap_target;
                CommitLineSegment(line_start_, line_end_, line_start_snap_, snap_target);
                line_start_ = line_end_;
                line_start_snap_ = {};
                line_start_snap_.world = line_start_;
                line_start_snap_.valid = true;
                line_start_snap_.snapped_to_geometry = true;
                focused_line_field_ = 1;
            }

            if (is_drawing_line_) {
                line_end_ = snap_target.world;
                line_end_snap_ = snap_target;
                DrawLine3D(draw_list, line_start_, line_end_, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);
            }

            if (is_drawing_line_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                CancelLineDrawing();
            }

            if (is_drawing_line_ && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                focused_line_field_ = 1 - focused_line_field_;
            }

            if (is_drawing_line_ && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                CommitLineSegment(line_start_, line_end_, line_start_snap_, line_end_snap_);
                is_drawing_line_ = false;
                awaiting_line_start_ = true;
                focused_line_field_ = 1;
                current_line_group_id_ = -1;
                line_start_snap_ = {};
                line_end_snap_ = {};
            }
        } else if (active_sketch_tool_ == SketchToolMode::Rectangle) {
            if (!is_drawing_rectangle_ && awaiting_rectangle_start_ && left_clicked) {
                rectangle_start_ = snapped_pos;
                rectangle_end_ = snapped_pos;
                is_drawing_rectangle_ = true;
                awaiting_rectangle_start_ = false;
                started_new_shape = true;
            }

            if (is_drawing_rectangle_) {
                rectangle_end_ = snapped_pos;

                Vec3 a = rectangle_start_;
                Vec3 b = rectangle_start_;
                Vec3 c = rectangle_end_;
                Vec3 d = rectangle_end_;
                switch (active_sketch_plane_) {
                    case SketchPlane_XY:
                    {
                        const float min_x = (std::min)(rectangle_start_.x, rectangle_end_.x);
                        const float max_x = (std::max)(rectangle_start_.x, rectangle_end_.x);
                        const float min_y = (std::min)(rectangle_start_.y, rectangle_end_.y);
                        const float max_y = (std::max)(rectangle_start_.y, rectangle_end_.y);
                        a = {min_x, min_y, 0.0f};
                        b = {max_x, min_y, 0.0f};
                        c = {max_x, max_y, 0.0f};
                        d = {min_x, max_y, 0.0f};
                        a.z = b.z = c.z = d.z = 0.0f;
                        break;
                    }
                    case SketchPlane_YZ:
                    {
                        const float min_y = (std::min)(rectangle_start_.y, rectangle_end_.y);
                        const float max_y = (std::max)(rectangle_start_.y, rectangle_end_.y);
                        const float min_z = (std::min)(rectangle_start_.z, rectangle_end_.z);
                        const float max_z = (std::max)(rectangle_start_.z, rectangle_end_.z);
                        a = {0.0f, min_y, min_z};
                        b = {0.0f, max_y, min_z};
                        c = {0.0f, max_y, max_z};
                        d = {0.0f, min_y, max_z};
                        a.x = b.x = c.x = d.x = 0.0f;
                        break;
                    }
                    case SketchPlane_XZ:
                    {
                        const float min_x = (std::min)(rectangle_start_.x, rectangle_end_.x);
                        const float max_x = (std::max)(rectangle_start_.x, rectangle_end_.x);
                        const float min_z = (std::min)(rectangle_start_.z, rectangle_end_.z);
                        const float max_z = (std::max)(rectangle_start_.z, rectangle_end_.z);
                        a = {min_x, 0.0f, min_z};
                        b = {max_x, 0.0f, min_z};
                        c = {max_x, 0.0f, max_z};
                        d = {min_x, 0.0f, max_z};
                        a.y = b.y = c.y = d.y = 0.0f;
                        break;
                    }
                    case SketchPlane_None:
                    default:
                        break;
                }

                DrawLine3D(draw_list, a, b, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);
                DrawLine3D(draw_list, b, c, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);
                DrawLine3D(draw_list, c, d, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);
                DrawLine3D(draw_list, d, a, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);

                if ((left_clicked && !started_new_shape) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    const int group_id = next_segment_group_++;
                    committed_line_segments_.push_back({a, b});
                    segment_group_ids_.push_back(group_id);
                    committed_line_segments_.push_back({b, c});
                    segment_group_ids_.push_back(group_id);
                    committed_line_segments_.push_back({c, d});
                    segment_group_ids_.push_back(group_id);
                    committed_line_segments_.push_back({d, a});
                    segment_group_ids_.push_back(group_id);
                    is_drawing_rectangle_ = false;
                    awaiting_rectangle_start_ = true;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    CancelLineDrawing();
                }
            }
        } else if (active_sketch_tool_ == SketchToolMode::Circle) {
            if (!is_drawing_circle_ && awaiting_circle_start_ && left_clicked) {
                circle_center_ = snapped_pos;
                circle_edge_ = snapped_pos;
                is_drawing_circle_ = true;
                awaiting_circle_start_ = false;
                started_new_shape = true;
            }

            if (is_drawing_circle_) {
                circle_edge_ = snapped_pos;
                float radius = 0.0f;
                switch (active_sketch_plane_) {
                    case SketchPlane_XY:
                        radius = std::sqrt((circle_edge_.x - circle_center_.x) * (circle_edge_.x - circle_center_.x) + (circle_edge_.y - circle_center_.y) * (circle_edge_.y - circle_center_.y));
                        break;
                    case SketchPlane_YZ:
                        radius = std::sqrt((circle_edge_.y - circle_center_.y) * (circle_edge_.y - circle_center_.y) + (circle_edge_.z - circle_center_.z) * (circle_edge_.z - circle_center_.z));
                        break;
                    case SketchPlane_XZ:
                        radius = std::sqrt((circle_edge_.x - circle_center_.x) * (circle_edge_.x - circle_center_.x) + (circle_edge_.z - circle_center_.z) * (circle_edge_.z - circle_center_.z));
                        break;
                    case SketchPlane_None:
                    default:
                        break;
                }

                DrawCircle3D(draw_list, circle_center_, radius, IM_COL32(100, 200, 255, 255), 2.0f, canvas_pos, canvas_size);

                if ((left_clicked && !started_new_shape) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    const int segments = 48;
                    Vec3 axis_u = {1.0f, 0.0f, 0.0f};
                    Vec3 axis_v = {0.0f, 1.0f, 0.0f};
                    switch (active_sketch_plane_) {
                        case SketchPlane_XY:
                            axis_u = {1.0f, 0.0f, 0.0f};
                            axis_v = {0.0f, 1.0f, 0.0f};
                            break;
                        case SketchPlane_YZ:
                            axis_u = {0.0f, 1.0f, 0.0f};
                            axis_v = {0.0f, 0.0f, 1.0f};
                            break;
                        case SketchPlane_XZ:
                            axis_u = {1.0f, 0.0f, 0.0f};
                            axis_v = {0.0f, 0.0f, 1.0f};
                            break;
                        case SketchPlane_None:
                        default:
                            break;
                    }

                    Vec3 previous = Add(Add(circle_center_, Mul(axis_u, radius)), Mul(axis_v, 0.0f));
                    const int group_id = next_segment_group_++;
                    for (int i = 1; i <= segments; ++i) {
                        const float t = (float)i / (float)segments;
                        const float angle = t * 6.2831853f;
                        const Vec3 point = Add(circle_center_, Add(Mul(axis_u, std::cos(angle) * radius), Mul(axis_v, std::sin(angle) * radius)));
                        committed_line_segments_.push_back({previous, point});
                        segment_group_ids_.push_back(group_id);
                        previous = point;
                    }
                    is_drawing_circle_ = false;
                    awaiting_circle_start_ = true;
                }

                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    CancelLineDrawing();
                }
            }
        }
    }

    // Render line drawing input fields as ImGui overlays
    if (sketch_mode_active_ && active_sketch_tool_ == SketchToolMode::Line && is_drawing_line_) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##line_input_overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
        ImGui::PopStyleVar(2);

        ImDrawList* overlay_draw_list = ImGui::GetWindowDrawList();

        // Calculate line length and angle
        const float dx = line_end_.x - line_start_.x;
        const float dy = line_end_.y - line_start_.y;
        const float dz = line_end_.z - line_start_.z;
        const float line_length = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float line_angle = std::atan2(dy, dx) * (180.0f / Pi);

        // Project start point to screen
        ImVec2 start_screen;
        if (ProjectToScreen(line_start_, canvas_pos, canvas_size, start_screen)) {
            // Render angle input field at start point
            const char* angle_label = "Angle (deg):";
            const ImVec2 angle_label_size = ImGui::CalcTextSize(angle_label);
            const ImVec2 angle_pos(start_screen.x - angle_label_size.x - 12.0f, start_screen.y - 20.0f);

            overlay_draw_list->AddRectFilled(
                ImVec2(angle_pos.x - 6.0f, angle_pos.y - 4.0f),
                ImVec2(angle_pos.x + angle_label_size.x + 4.0f, angle_pos.y + angle_label_size.y + 4.0f),
                IM_COL32(22, 28, 36, 220),
                3.0f
            );
            overlay_draw_list->AddRect(
                ImVec2(angle_pos.x - 6.0f, angle_pos.y - 4.0f),
                ImVec2(angle_pos.x + angle_label_size.x + 4.0f, angle_pos.y + angle_label_size.y + 4.0f),
                focused_line_field_ == 0 ? IM_COL32(100, 200, 255, 255) : IM_COL32(100, 200, 255, 150),
                3.0f,
                0,
                focused_line_field_ == 0 ? 2.0f : 1.0f
            );
            overlay_draw_list->AddText(angle_pos, IM_COL32(235, 240, 250, 255), angle_label);

            // Display angle value
            char angle_text[32];
            std::snprintf(angle_text, sizeof(angle_text), "%.1f°", line_angle);
            const ImVec2 angle_value_pos(angle_pos.x, angle_pos.y + angle_label_size.y + 8.0f);
            overlay_draw_list->AddRectFilled(
                ImVec2(angle_value_pos.x - 6.0f, angle_value_pos.y - 4.0f),
                ImVec2(angle_value_pos.x + 40.0f, angle_value_pos.y + angle_label_size.y + 4.0f),
                IM_COL32(10, 12, 16, 230),
                3.0f
            );
            overlay_draw_list->AddRect(
                ImVec2(angle_value_pos.x - 6.0f, angle_value_pos.y - 4.0f),
                ImVec2(angle_value_pos.x + 40.0f, angle_value_pos.y + angle_label_size.y + 4.0f),
                focused_line_field_ == 0 ? IM_COL32(100, 200, 255, 255) : IM_COL32(100, 150, 200, 150),
                3.0f,
                0,
                focused_line_field_ == 0 ? 2.0f : 1.0f
            );
            overlay_draw_list->AddText(angle_value_pos, IM_COL32(200, 220, 255, 255), angle_text);
        }

        // Project midpoint to screen
        const Vec3 midpoint = {
            (line_start_.x + line_end_.x) * 0.5f,
            (line_start_.y + line_end_.y) * 0.5f,
            (line_start_.z + line_end_.z) * 0.5f
        };
        ImVec2 midpoint_screen;
        if (ProjectToScreen(midpoint, canvas_pos, canvas_size, midpoint_screen)) {
            // Render length input field at midpoint
            const char* length_label = "Length:";
            const ImVec2 length_label_size = ImGui::CalcTextSize(length_label);
            const ImVec2 length_pos(midpoint_screen.x + 12.0f, midpoint_screen.y - 20.0f);

            overlay_draw_list->AddRectFilled(
                ImVec2(length_pos.x - 6.0f, length_pos.y - 4.0f),
                ImVec2(length_pos.x + length_label_size.x + 4.0f, length_pos.y + length_label_size.y + 4.0f),
                IM_COL32(22, 28, 36, 220),
                3.0f
            );
            overlay_draw_list->AddRect(
                ImVec2(length_pos.x - 6.0f, length_pos.y - 4.0f),
                ImVec2(length_pos.x + length_label_size.x + 4.0f, length_pos.y + length_label_size.y + 4.0f),
                focused_line_field_ == 1 ? IM_COL32(100, 200, 255, 255) : IM_COL32(100, 200, 255, 150),
                3.0f,
                0,
                focused_line_field_ == 1 ? 2.0f : 1.0f
            );
            overlay_draw_list->AddText(length_pos, IM_COL32(235, 240, 250, 255), length_label);

            // Display length value
            char length_text[32];
            std::snprintf(length_text, sizeof(length_text), "%.3f", line_length);
            const ImVec2 length_value_pos(length_pos.x, length_pos.y + length_label_size.y + 8.0f);
            overlay_draw_list->AddRectFilled(
                ImVec2(length_value_pos.x - 6.0f, length_value_pos.y - 4.0f),
                ImVec2(length_value_pos.x + 50.0f, length_value_pos.y + length_label_size.y + 4.0f),
                IM_COL32(10, 12, 16, 230),
                3.0f
            );
            overlay_draw_list->AddRect(
                ImVec2(length_value_pos.x - 6.0f, length_value_pos.y - 4.0f),
                ImVec2(length_value_pos.x + 50.0f, length_value_pos.y + length_label_size.y + 4.0f),
                focused_line_field_ == 1 ? IM_COL32(100, 200, 255, 255) : IM_COL32(100, 150, 200, 150),
                3.0f,
                0,
                focused_line_field_ == 1 ? 2.0f : 1.0f
            );
            overlay_draw_list->AddText(length_value_pos, IM_COL32(200, 220, 255, 255), length_text);
        }

        ImGui::End();
    }

    if (show_axes_) {
        DrawLine3D(draw_list, {-8.0f, 0.0f, 0.0f}, {8.0f, 0.0f, 0.0f}, IM_COL32(220, 90, 90, 255), 2.5f, canvas_pos, canvas_size);
        DrawLine3D(draw_list, {0.0f, -8.0f, 0.0f}, {0.0f, 8.0f, 0.0f}, IM_COL32(90, 210, 120, 255), 2.5f, canvas_pos, canvas_size);
        DrawLine3D(draw_list, {0.0f, 0.0f, -8.0f}, {0.0f, 0.0f, 8.0f}, IM_COL32(100, 170, 230, 255), 2.5f, canvas_pos, canvas_size);
    }

    const double seconds_since_scroll = ImGui::GetTime() - info_last_scroll_time_;
    float info_alpha = 0.0f;
    if (seconds_since_scroll <= info_hold_seconds) {
        info_alpha = 1.0f;
    } else if (seconds_since_scroll <= info_hold_seconds + info_fade_seconds) {
        info_alpha = 1.0f - (float)((seconds_since_scroll - info_hold_seconds) / info_fade_seconds);
    }

    if (show_hud_ && info_alpha > 0.0f) {
        char info_text[160];
        std::snprintf(
            info_text,
            sizeof(info_text),
                "Dist %.2f\nYaw %.1f\nPitch %.1f\nWheel: dolly\n%s Drag: orbit\n%s Drag: pan",
                distance_,
                yaw_ * (180.0f / Pi),
                pitch_ * (180.0f / Pi),
            MouseButtonName(orbit_button_),
            MouseButtonName(drag_button_)
        );

        const ImVec2 info_pos(canvas_pos.x + 12.0f, canvas_pos.y + 12.0f);
        const ImVec2 info_size = ImGui::CalcTextSize(info_text);
        const int bg_alpha = (int)(210.0f * info_alpha);
        const int border_alpha = (int)(120.0f * info_alpha);
        const int text_alpha = (int)(255.0f * info_alpha);

        draw_list->AddRectFilled(
            ImVec2(info_pos.x - 8.0f, info_pos.y - 6.0f),
            ImVec2(info_pos.x + info_size.x + 8.0f, info_pos.y + info_size.y + 6.0f),
            IM_COL32(10, 12, 16, bg_alpha),
            6.0f
        );
        draw_list->AddRect(
            ImVec2(info_pos.x - 8.0f, info_pos.y - 6.0f),
            ImVec2(info_pos.x + info_size.x + 8.0f, info_pos.y + info_size.y + 6.0f),
            IM_COL32(90, 100, 120, border_alpha),
            6.0f
        );
        draw_list->AddText(info_pos, IM_COL32(235, 240, 250, text_alpha), info_text);
    }

    if (awaiting_sketch_plane_selection_) {
        const char* pick_text = "Select a sketch plane";
        const ImVec2 pick_size = ImGui::CalcTextSize(pick_text);
        const ImVec2 pick_pos(canvas_pos.x + 12.0f, canvas_end.y - pick_size.y - 18.0f);
        draw_list->AddRectFilled(
            ImVec2(pick_pos.x - 8.0f, pick_pos.y - 6.0f),
            ImVec2(pick_pos.x + pick_size.x + 8.0f, pick_pos.y + pick_size.y + 6.0f),
            IM_COL32(22, 28, 36, 210),
            5.0f
        );
        draw_list->AddRect(
            ImVec2(pick_pos.x - 8.0f, pick_pos.y - 6.0f),
            ImVec2(pick_pos.x + pick_size.x + 8.0f, pick_pos.y + pick_size.y + 6.0f),
            IM_COL32(255, 166, 84, 180),
            5.0f
        );
        draw_list->AddText(pick_pos, IM_COL32(255, 230, 205, 255), pick_text);
    }

    draw_list->PopClipRect();
    draw_list->AddRect(canvas_pos, canvas_end, IM_COL32(80, 90, 110, 255));

    ImGui::End();

    const ImGuiStyle& style = ImGui::GetStyle();
    const float btn_view_w = (camera_icon_texture_ != (ImTextureID)0)
        ? (ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f)
        : (ImGui::CalcTextSize("View Type").x + style.FramePadding.x * 2.0f);
    const float btn_grid_w = (grid_icon_texture_ != (ImTextureID)0)
        ? (ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f)
        : (ImGui::CalcTextSize("Layout Grid").x + style.FramePadding.x * 2.0f);
    const float btn_hud_w = ImGui::CalcTextSize(show_hud_ ? "HUD On" : "HUD Off").x + style.FramePadding.x * 2.0f;
    const float btn_reset_w = ImGui::CalcTextSize("Reset View").x + style.FramePadding.x * 2.0f;
    const float btn_cfg_w = (settings_icon_texture_ != (ImTextureID)0)
        ? (ImGui::GetFrameHeight() + style.FramePadding.x * 2.0f)
        : (ImGui::CalcTextSize("Cfg").x + style.FramePadding.x * 2.0f);
    const float label_w = ImGui::CalcTextSize("Viewport Options").x;

    const float button_row_width = btn_view_w + btn_grid_w + btn_hud_w + btn_reset_w + btn_cfg_w + style.ItemSpacing.x * 6.0f;
    float options_width = button_row_width + style.WindowPadding.x * 2.0f;
    const float min_label_width = label_w + style.WindowPadding.x * 2.0f + 8.0f;
    if (options_width < min_label_width) {
        options_width = min_label_width;
    }

    const float max_overlay_width = canvas_size.x - 8.0f;
    if (options_width > max_overlay_width) {
        options_width = max_overlay_width;
    }

    float options_height = style.WindowPadding.y * 2.0f + ImGui::GetFrameHeight() + style.ItemSpacing.y + ImGui::GetTextLineHeight() + 4.0f;

    ImVec2 options_pos(canvas_pos.x + 12.0f, canvas_end.y - options_height - 12.0f);
    if (options_follow_viewport_) {
        switch (options_anchor_dir_) {
            case ImGuiDir_Up:
                options_pos = ImVec2(canvas_pos.x + (canvas_size.x - options_width) * 0.5f, canvas_pos.y + 12.0f);
                break;
            case ImGuiDir_Down:
                options_pos = ImVec2(canvas_pos.x + (canvas_size.x - options_width) * 0.5f, canvas_end.y - options_height - 12.0f);
                break;
            case ImGuiDir_Left:
                options_pos = ImVec2(canvas_pos.x + 12.0f, canvas_pos.y + 12.0f);
                break;
            case ImGuiDir_Right:
                options_pos = ImVec2(canvas_end.x - options_width - 12.0f, canvas_pos.y + 12.0f);
                break;
            default:
                break;
        }

        if (options_pos.x < canvas_pos.x + 4.0f) {
            options_pos.x = canvas_pos.x + 4.0f;
        }
        if (options_pos.y < canvas_pos.y + 4.0f) {
            options_pos.y = canvas_pos.y + 4.0f;
        }
        if (options_pos.x + options_width > canvas_end.x - 4.0f) {
            options_pos.x = canvas_end.x - options_width - 4.0f;
        }
        if (options_pos.y + options_height > canvas_end.y - 4.0f) {
            options_pos.y = canvas_end.y - options_height - 4.0f;
        }
    }

    if (options_follow_viewport_ || options_snap_requested_) {
        ImGui::SetNextWindowPos(options_pos, ImGuiCond_Always);
        options_snap_requested_ = false;
    } else {
        ImGui::SetNextWindowPos(options_pos, ImGuiCond_FirstUseEver);
    }

    ImGui::SetNextWindowSize(ImVec2(options_width, options_height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGuiWindowFlags options_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("Viewport Options", nullptr, options_flags);

    if (camera_icon_texture_ != (ImTextureID)0) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 67, 90, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(67, 92, 124, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 108, 145, 255));
        const float icon_size = ImGui::GetFrameHeight() - style.FramePadding.y * 2.0f;
        if (ImGui::ImageButton("##view_render_type_cfg", camera_icon_texture_, ImVec2(icon_size, icon_size))) {
            ImGui::OpenPopup("view_render_type_popup");
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("View Render Type");
        }
    } else {
        if (ImGui::Button("view Type")) {
            ImGui::OpenPopup("view_render_type_popup");
        }
    }
    if (ImGui::BeginPopup("view_render_type_popup")) {
        ImGui::TextUnformatted("View Render Type");
        ImGui::Separator();

        bool ortho_checked = ortho_mode_;
        if (ImGui::Checkbox("Orthographic Mode", &ortho_checked) && ortho_checked) {
            ortho_mode_ = true;
            persp_mode_ = false;
            persp_w_ortho_faces_mode_ = false;
        }

        bool persp_checked = persp_mode_;
        if (ImGui::Checkbox("Perspective Mode", &persp_checked) && persp_checked) {
            ortho_mode_ = false;
            persp_mode_ = true;
            persp_w_ortho_faces_mode_ = false;
        }

        bool persp_ortho_checked = persp_w_ortho_faces_mode_;
        if (ImGui::Checkbox("Perspective With Ortho Faces", &persp_ortho_checked) && persp_ortho_checked) {
            ortho_mode_ = false;
            persp_mode_ = false;
            persp_w_ortho_faces_mode_ = true;
        }

        if (!ortho_mode_ && !persp_mode_ && !persp_w_ortho_faces_mode_) {
            persp_mode_ = true;
        }

        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (grid_icon_texture_ != (ImTextureID)0) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 67, 90, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(67, 92, 124, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 108, 145, 255));
        const float icon_size = ImGui::GetFrameHeight() - style.FramePadding.y * 2.0f;
        if (ImGui::ImageButton("##grd_cfg", grid_icon_texture_, ImVec2(icon_size, icon_size))) {
            ImGui::OpenPopup("grid_settings_popup");
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Layout Grid");
        }
    } else {
        if (ImGui::Button("Layout Grid")) {
            ImGui::OpenPopup("grid_settings_popup");
        }
    }
    if (ImGui::BeginPopup("grid_settings_popup")) {
        ImGui::TextUnformatted("Layout Grid");
        ImGui::Separator();

        bool layout_grid_checked = show_layout_grid_;
        if (ImGui::Checkbox("Show Layout Grid", &layout_grid_checked)) {
            SetLayoutGridVisible(layout_grid_checked);
        }
        if (ImGui::Checkbox("Show Axes", &show_axes_)) {
            // No additional action needed, axes visibility is handled in the render loop.
        }

        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(show_hud_ ? "HUD On" : "HUD Off")) {
        show_hud_ = !show_hud_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        target_ = {0.0f, 0.0f, 0.0f};
        yaw_ = 0.785f;
        pitch_ = 0.65f;
        distance_ = 26.0f;
    }

    ImGui::SameLine();
    if (settings_icon_texture_ != (ImTextureID)0) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(49, 67, 90, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(67, 92, 124, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 108, 145, 255));
        const float icon_size = ImGui::GetFrameHeight() - style.FramePadding.y * 2.0f;
        if (ImGui::ImageButton("##viewport_options_cfg", settings_icon_texture_, ImVec2(icon_size, icon_size))) {
            ImGui::OpenPopup("viewport_options_settings_popup");
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Viewport Options Settings");
        }
    } else {
        if (ImGui::Button("Cfg")) {
            ImGui::OpenPopup("viewport_options_settings_popup");
        }
    }
    if (ImGui::BeginPopup("viewport_options_settings_popup")) {
        ImGui::TextUnformatted("Attachment");
        ImGui::Separator();
         if (ImGui::Selectable(options_follow_viewport_ ? "Detach From Viewport" : "Follow Viewport")) {
            options_follow_viewport_ = !options_follow_viewport_;
            options_snap_requested_ = true;
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Anchor Position");
        ImGui::Separator();
        if (ImGui::Selectable("Top Middle")) {
            options_anchor_dir_ = ImGuiDir_Up;
            options_follow_viewport_ = true;
            options_snap_requested_ = true;
        }
        if (ImGui::Selectable("Bottom Middle")) {
            options_anchor_dir_ = ImGuiDir_Down;
            options_follow_viewport_ = true;
            options_snap_requested_ = true;
        }
        if (ImGui::Selectable("Left")) {
            options_anchor_dir_ = ImGuiDir_Left;
            options_follow_viewport_ = true;
            options_snap_requested_ = true;
        }
        if (ImGui::Selectable("Right")) {
            options_anchor_dir_ = ImGuiDir_Right;
            options_follow_viewport_ = true;
            options_snap_requested_ = true;
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    const char* options_label = "Viewport Options";
    const float label_width = ImGui::CalcTextSize(options_label).x;
    const float centered_x = (ImGui::GetContentRegionAvail().x - label_width) * 0.5f;
    if (centered_x > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + centered_x);
    }
    ImGui::TextUnformatted(options_label);

    ImGui::End();
}

ViewportWindow::Vec3 ViewportWindow::ScreenToWorldPlane(const ImVec2& screen_pos, const ImVec2& canvas_pos, const ImVec2& canvas_size, SketchPlane plane) const {
    if (plane == SketchPlane_None) {
        return {0.0f, 0.0f, 0.0f};
    }

    const ImVec2 center(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f);
    const ImVec2 rel_screen(screen_pos.x - center.x, screen_pos.y - center.y);

    const Vec3 cam_right = CameraRight();
    const Vec3 cam_up = CameraUp();
    const Vec3 cam_forward = CameraForward();

    if (ortho_mode_) {
        const float ortho_scale = (distance_ * 1.8f) / (canvas_size.y > 0.0f ? canvas_size.y : 1.0f);
        Vec3 world = Add(Mul(cam_right, rel_screen.x * ortho_scale), Mul(cam_up, -rel_screen.y * ortho_scale));
        world = Add(world, target_);

        // Clamp to the active sketch plane
        switch (plane) {
            case SketchPlane_XY:
                world.z = 0.0f;
                break;
            case SketchPlane_YZ:
                world.x = 0.0f;
                break;
            case SketchPlane_XZ:
                world.y = 0.0f;
                break;
            case SketchPlane_None:
            default:
                break;
        }
        return world;
    }

    const float fov_rad = 55.0f * (Pi / 180.0f);
    const float focal = (canvas_size.y * 0.5f) / std::tan(fov_rad * 0.5f);
    Vec3 ray_dir = Normalize(Add(Add(Mul(cam_right, rel_screen.x / focal), Mul(cam_up, -rel_screen.y / focal)), cam_forward));
    const Vec3 ray_origin = CameraPosition();

    Vec3 plane_point = target_;
    Vec3 plane_normal = {0.0f, 0.0f, 1.0f};
    switch (plane) {
        case SketchPlane_XY:
            plane_point.z = 0.0f;
            plane_normal = {0.0f, 0.0f, 1.0f};
            break;
        case SketchPlane_YZ:
            plane_point.x = 0.0f;
            plane_normal = {1.0f, 0.0f, 0.0f};
            break;
        case SketchPlane_XZ:
            plane_point.y = 0.0f;
            plane_normal = {0.0f, 1.0f, 0.0f};
            break;
        case SketchPlane_None:
        default:
            break;
    }

    const float denom = Dot(ray_dir, plane_normal);
    if (std::fabs(denom) < 1e-6f) {
        return plane_point;
    }

    const float t = Dot(Sub(plane_point, ray_origin), plane_normal) / denom;
    if (t < 0.0f) {
        return plane_point;
    }

    return Add(ray_origin, Mul(ray_dir, t));
}

ViewportWindow::Vec3 ViewportWindow::ScreenToWorldPlane(const ImVec2& screen_pos, const ImVec2& canvas_pos, const ImVec2& canvas_size) const {
    return ScreenToWorldPlane(screen_pos, canvas_pos, canvas_size, active_sketch_plane_);
}

ViewportWindow::Vec3 ViewportWindow::SnapToGrid(const Vec3& world_pos) const {
    if (active_sketch_plane_ == SketchPlane_None) {
        return world_pos;
    }

    const float world_per_pixel = (distance_ * 1.8f) / (last_canvas_size_.y > 0.0f ? last_canvas_size_.y : 1.0f);
    const float desired_step = world_per_pixel * 48.0f;
    const float step = NiceGridStep(desired_step);

    Vec3 snapped = world_pos;

    switch (active_sketch_plane_) {
        case SketchPlane_XY:
            snapped.x = std::round(world_pos.x / step) * step;
            snapped.y = std::round(world_pos.y / step) * step;
            snapped.z = 0.0f;
            break;
        case SketchPlane_YZ:
            snapped.y = std::round(world_pos.y / step) * step;
            snapped.z = std::round(world_pos.z / step) * step;
            snapped.x = 0.0f;
            break;
        case SketchPlane_XZ:
            snapped.x = std::round(world_pos.x / step) * step;
            snapped.z = std::round(world_pos.z / step) * step;
            snapped.y = 0.0f;
            break;
        case SketchPlane_None:
        default:
            break;
    }

    return snapped;
}

ViewportWindow::SnapTarget ViewportWindow::ResolveSnapTarget(const ImVec2& mouse_screen, const ImVec2& canvas_pos, const ImVec2& canvas_size) const {
    SnapTarget result;
    if (active_sketch_plane_ == SketchPlane_None) {
        return result;
    }

    constexpr float kSnapPixelThreshold = 18.0f;
    const float snap_threshold_sq = kSnapPixelThreshold * kSnapPixelThreshold;
    float best_dist_sq = snap_threshold_sq;
    bool has_best = false;

    const Vec3 mouse_world = ScreenToWorldPlane(mouse_screen, canvas_pos, canvas_size);
    const Vec3 grid_world = SnapToGrid(mouse_world);

    ImVec2 grid_screen;
    if (ProjectToScreen(grid_world, canvas_pos, canvas_size, grid_screen)) {
        const float dx = grid_screen.x - mouse_screen.x;
        const float dy = grid_screen.y - mouse_screen.y;
        const float dist_sq = dx * dx + dy * dy;
        result.world = grid_world;
        result.valid = true;
        result.snapped_to_grid = true;
        best_dist_sq = dist_sq;
        has_best = true;
    }

    auto score_candidate = [&](const Vec3& world, bool snapped_to_segment_interior, int segment_index, float segment_t) {
        ImVec2 screen_pos;
        if (!ProjectToScreen(world, canvas_pos, canvas_size, screen_pos)) {
            return;
        }

        const float dx = screen_pos.x - mouse_screen.x;
        const float dy = screen_pos.y - mouse_screen.y;
        const float dist_sq = dx * dx + dy * dy;
        if (!has_best || dist_sq <= best_dist_sq + 0.25f) {
            result.world = world;
            result.valid = true;
            result.snapped_to_segment_interior = snapped_to_segment_interior;
            result.segment_index = segment_index;
            result.segment_t = segment_t;
            result.snapped_to_geometry = true;
            result.snapped_to_grid = false;
            best_dist_sq = dist_sq;
            has_best = true;
        }
    };

    // Geometry snaps should override the grid only when they're close enough.
    for (int i = 0; i < static_cast<int>(committed_line_segments_.size()); ++i) {
        const LineSegment& segment = committed_line_segments_[i];
        const ImVec2 a2 = ProjectPlanePoint(segment.start, active_sketch_plane_);
        const ImVec2 b2 = ProjectPlanePoint(segment.end, active_sketch_plane_);
        const ImVec2 m2 = ProjectPlanePoint(mouse_world, active_sketch_plane_);

        float t = 0.0f;
        const ImVec2 closest2 = ClosestPointOnSegment2D(m2, a2, b2, &t);
        const Vec3 closest_world = UnprojectPlanePoint(closest2, active_sketch_plane_);
        const bool interior = (t > 0.001f && t < 0.999f);
        score_candidate(closest_world, interior, i, t);
    }

    // Endpoints are also geometry snaps, score them separately so they win over segment interiors.
    for (int i = 0; i < static_cast<int>(committed_line_segments_.size()); ++i) {
        const LineSegment& segment = committed_line_segments_[i];
        score_candidate(segment.start, false, i, 0.0f);
        score_candidate(segment.end, false, i, 1.0f);
    }

    return result;
}

void ViewportWindow::SplitSegmentAtPoint(int segment_index, const Vec3& point) {
    if (segment_index < 0 || segment_index >= static_cast<int>(committed_line_segments_.size())) {
        return;
    }
    if (segment_index >= static_cast<int>(segment_group_ids_.size())) {
        return;
    }

    const LineSegment segment = committed_line_segments_[segment_index];
    if (DistSq(segment.start, point) < 1e-6f || DistSq(segment.end, point) < 1e-6f) {
        return;
    }

    const int group_id = segment_group_ids_[segment_index];
    committed_line_segments_.erase(committed_line_segments_.begin() + segment_index);
    segment_group_ids_.erase(segment_group_ids_.begin() + segment_index);

    committed_line_segments_.insert(committed_line_segments_.begin() + segment_index, {segment.start, point});
    segment_group_ids_.insert(segment_group_ids_.begin() + segment_index, group_id);

    committed_line_segments_.insert(committed_line_segments_.begin() + segment_index + 1, {point, segment.end});
    segment_group_ids_.insert(segment_group_ids_.begin() + segment_index + 1, group_id);
}

void ViewportWindow::CommitLineSegment(const Vec3& start, const Vec3& end, const SnapTarget& start_snap, const SnapTarget& end_snap) {
    if (DistSq(start, end) < 1e-6f) {
        return;
    }

    auto split_if_needed = [&](const SnapTarget& snap) {
        if (!snap.valid || !snap.snapped_to_segment_interior || snap.segment_index < 0) {
            return;
        }
        SplitSegmentAtPoint(snap.segment_index, snap.world);
    };

    if (start_snap.snapped_to_segment_interior && end_snap.snapped_to_segment_interior && start_snap.segment_index == end_snap.segment_index) {
        if (start_snap.segment_index >= 0 && start_snap.segment_index < static_cast<int>(committed_line_segments_.size()) &&
            start_snap.segment_index < static_cast<int>(segment_group_ids_.size())) {
            const LineSegment segment = committed_line_segments_[start_snap.segment_index];
            const int group_id = segment_group_ids_[start_snap.segment_index];
            Vec3 first = start;
            Vec3 second = end;
            float first_t = start_snap.segment_t;
            float second_t = end_snap.segment_t;
            if (second_t < first_t) {
                std::swap(first, second);
                std::swap(first_t, second_t);
            }

            committed_line_segments_.erase(committed_line_segments_.begin() + start_snap.segment_index);
            segment_group_ids_.erase(segment_group_ids_.begin() + start_snap.segment_index);

            committed_line_segments_.insert(committed_line_segments_.begin() + start_snap.segment_index, {segment.start, first});
            segment_group_ids_.insert(segment_group_ids_.begin() + start_snap.segment_index, group_id);
            committed_line_segments_.insert(committed_line_segments_.begin() + start_snap.segment_index + 1, {first, second});
            segment_group_ids_.insert(segment_group_ids_.begin() + start_snap.segment_index + 1, group_id);
            committed_line_segments_.insert(committed_line_segments_.begin() + start_snap.segment_index + 2, {second, segment.end});
            segment_group_ids_.insert(segment_group_ids_.begin() + start_snap.segment_index + 2, group_id);
        }
    } else {
        if (start_snap.snapped_to_segment_interior && end_snap.snapped_to_segment_interior && start_snap.segment_index != end_snap.segment_index) {
            if (start_snap.segment_index > end_snap.segment_index) {
                split_if_needed(start_snap);
                split_if_needed(end_snap);
            } else {
                split_if_needed(end_snap);
                split_if_needed(start_snap);
            }
        } else {
            split_if_needed(start_snap);
            split_if_needed(end_snap);
        }
    }

    committed_line_segments_.push_back({start, end});
    segment_group_ids_.push_back(current_line_group_id_ != -1 ? current_line_group_id_ : next_segment_group_++);

    if (start_snap.snapped_to_segment_interior && end_snap.snapped_to_segment_interior && start_snap.segment_index == end_snap.segment_index) {
        // Keep the added line in the same group as the split geometry when tracing along an existing edge.
        if (!segment_group_ids_.empty()) {
            segment_group_ids_.back() = current_line_group_id_ != -1 ? current_line_group_id_ : next_segment_group_ - 1;
        }
    }
}

void ViewportWindow::CancelLineDrawing() {
    active_sketch_tool_ = SketchToolMode::None;
    is_drawing_line_ = false;
    awaiting_line_start_ = false;
    line_start_ = {0.0f, 0.0f, 0.0f};
    line_end_ = {0.0f, 0.0f, 0.0f};
    line_start_snap_ = {};
    line_end_snap_ = {};
    focused_line_field_ = 1;
    current_line_group_id_ = -1;
    crosshair_enabled_ = false;

    is_drawing_rectangle_ = false;
    awaiting_rectangle_start_ = false;
    rectangle_start_ = {0.0f, 0.0f, 0.0f};
    rectangle_end_ = {0.0f, 0.0f, 0.0f};

    is_drawing_circle_ = false;
    awaiting_circle_start_ = false;
    circle_center_ = {0.0f, 0.0f, 0.0f};
    circle_edge_ = {0.0f, 0.0f, 0.0f};
}

void ViewportWindow::StartLineDrawing() {
    CancelLineDrawing();
    active_sketch_tool_ = SketchToolMode::Line;
    is_drawing_line_ = false;
    awaiting_line_start_ = true;
    line_start_ = {0.0f, 0.0f, 0.0f};
    line_end_ = {0.0f, 0.0f, 0.0f};
    line_start_snap_ = {};
    line_end_snap_ = {};
    focused_line_field_ = 1;
    current_line_group_id_ = -1;
    crosshair_enabled_ = true;
}

void ViewportWindow::StartRectangleDrawing() {
    CancelLineDrawing();
    active_sketch_tool_ = SketchToolMode::Rectangle;
    is_drawing_rectangle_ = false;
    awaiting_rectangle_start_ = true;
    rectangle_start_ = {0.0f, 0.0f, 0.0f};
    rectangle_end_ = {0.0f, 0.0f, 0.0f};
    crosshair_enabled_ = true;
}

void ViewportWindow::StartCircleDrawing() {
    CancelLineDrawing();
    active_sketch_tool_ = SketchToolMode::Circle;
    is_drawing_circle_ = false;
    awaiting_circle_start_ = true;
    circle_center_ = {0.0f, 0.0f, 0.0f};
    circle_edge_ = {0.0f, 0.0f, 0.0f};
    crosshair_enabled_ = true;
}

bool ViewportWindow::GetCommittedLineSegments(std::vector<LineSegment>* out_segments) {
    if (out_segments == nullptr) {
        return false;
    }
    if (committed_line_segments_.empty()) {
        return false;
    }
    *out_segments = committed_line_segments_;
    committed_line_segments_.clear();
    return true;
}

bool ViewportWindow::GetSelectedPolygonWorldPoints(std::vector<Vec3>* out_points) const {
    if (out_points == nullptr || selected_polygon_world_points_.size() < 3) {
        return false;
    }
    *out_points = selected_polygon_world_points_;
    return true;
}

bool ViewportWindow::GetSelectedFillIds(int* out_polygon_index, int* out_overlap_index) const {
    if (out_polygon_index != nullptr) {
        *out_polygon_index = selected_polygon_index_;
    }
    if (out_overlap_index != nullptr) {
        *out_overlap_index = selected_overlap_index_;
    }
    return selected_polygon_index_ != -1 || selected_overlap_index_ != -1;
}

void ViewportWindow::SetExtrudedBodyPreview(const std::vector<Vec3>& polygon_points, float depth_world) {
    extruded_body_preview_polygon_ = polygon_points;
    extruded_body_preview_depth_world_ = depth_world;
    extruded_body_preview_visible_ = polygon_points.size() >= 3;
}

void ViewportWindow::ClearExtrudedBodyPreview() {
    extruded_body_preview_polygon_.clear();
    extruded_body_preview_depth_world_ = 0.0f;
    extruded_body_preview_visible_ = false;
}

void ViewportWindow::SetExtrudedBodyFinal(const std::vector<Vec3>& polygon_points, float depth_world) {
    extruded_body_final_polygon_ = polygon_points;
    extruded_body_final_depth_world_ = depth_world;
    extruded_body_final_visible_ = polygon_points.size() >= 3;
}

void ViewportWindow::ClearExtrudedBodyFinal() {
    extruded_body_final_polygon_.clear();
    extruded_body_final_depth_world_ = 0.0f;
    extruded_body_final_visible_ = false;
}
