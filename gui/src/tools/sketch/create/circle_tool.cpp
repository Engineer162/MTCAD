#include "circle_tool.h"

#include "imgui.h"

#include <iostream>

namespace mtcad {

std::unique_ptr<ToolOptions> CircleTool::CreateOptions() const {
    return std::make_unique<CircleToolOptions>();
}

void CircleTool::RenderOptions(ToolOptions* options) {
    CircleToolOptions* o = dynamic_cast<CircleToolOptions*>(options);
    if (!o) {
        return;
    }
    ImGui::InputFloat("Radius", &o->radius);
}

void CircleTool::Apply(ToolOptions* options) {
    CircleToolOptions* o = dynamic_cast<CircleToolOptions*>(options);
    if (!o) return;
    std::cout << "Applying Circle tool: radius=" << o->radius << "\n";
}

} // namespace mtcad