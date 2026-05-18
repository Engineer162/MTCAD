#pragma once
#include <string>
#include <memory>
#include "imgui.h"

namespace mtcad {

enum class ToolMode { Solid, Sketch };
enum class ToolId { None = 0, Move, Extrude, Line, Rectangle, Circle };

struct ToolOptions {
    virtual ~ToolOptions() = default;
};

struct Tool {
    virtual ~Tool() = default;
    virtual ToolId Id() const = 0;
    virtual ToolMode Mode() const = 0;
    virtual std::unique_ptr<ToolOptions> CreateOptions() const = 0;
    // Render options UI into ImGui
    virtual void RenderOptions(ToolOptions* options) = 0;
    // Apply options (called when user clicks Apply)
    virtual void Apply(ToolOptions* options) = 0;
    // Cancel
    virtual void Cancel(ToolOptions* options) {}
    virtual std::string Name() const = 0;
};

} // namespace mtcad
