#pragma once

#include "tool.h"

namespace mtcad {

struct LineToolOptions : public ToolOptions {
    float length = 1.0f;
    float angle = 0.0f;
};

class LineTool : public Tool {
public:
    LineTool() = default;
    ToolId Id() const override { return ToolId::Line; }
    ToolMode Mode() const override { return ToolMode::Sketch; }
    std::string Name() const override { return "Line"; }
    std::unique_ptr<ToolOptions> CreateOptions() const override;

    void RenderOptions(ToolOptions* options) override;
    void Apply(ToolOptions* options) override;
    void Cancel(ToolOptions* options) override {}
};

} // namespace mtcad