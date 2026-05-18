#include "line_tool.h"
#include "imgui.h"
#include <iostream>

namespace mtcad {

std::unique_ptr<ToolOptions> LineTool::CreateOptions() const {
    return std::make_unique<LineToolOptions>();
}

void LineTool::RenderOptions(ToolOptions* options) {
    LineToolOptions* o = dynamic_cast<LineToolOptions*>(options);
    if (!o) {
        return;
    }
    ImGui::InputFloat("Length", &o->length);
    ImGui::InputFloat("Angle", &o->angle);
}

void LineTool::Apply(ToolOptions* options) {
    LineToolOptions* o = dynamic_cast<LineToolOptions*>(options);
    if (!o) return;
    // For now just log to console; integration point for actual command
    std::cout << "Applying Line tool: length=" << o->length << " angle=" << o->angle << "\n";
}

} // namespace mtcad
