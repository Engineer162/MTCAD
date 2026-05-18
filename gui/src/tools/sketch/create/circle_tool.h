#pragma once

#include "../../tool.h"

namespace mtcad {

struct CircleToolOptions : public ToolOptions {
    float radius = 1.0f;
};

class CircleTool : public Tool {
public:
    CircleTool() = default;
    ToolId Id() const override { return ToolId::Circle; }
    ToolMode Mode() const override { return ToolMode::Sketch; }
    std::string Name() const override { return "Circle"; }
    std::unique_ptr<ToolOptions> CreateOptions() const override;

    void RenderOptions(ToolOptions* options) override;
    void Apply(ToolOptions* options) override;
    void Cancel(ToolOptions* options) override {}
};

} // namespace mtcad