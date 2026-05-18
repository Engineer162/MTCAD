#pragma once

#include "../../tool.h"

namespace mtcad {

struct RectangleToolOptions : public ToolOptions {
    float width = 1.0f;
    float height = 1.0f;
};

class RectangleTool : public Tool {
public:
    RectangleTool() = default;
    ToolId Id() const override { return ToolId::Rectangle; }
    ToolMode Mode() const override { return ToolMode::Sketch; }
    std::string Name() const override { return "Rectangle"; }
    std::unique_ptr<ToolOptions> CreateOptions() const override;

    void RenderOptions(ToolOptions* options) override;
    void Apply(ToolOptions* options) override;
    void Cancel(ToolOptions* options) override {}
};

} // namespace mtcad