#include "rectangle_tool.h"

#include "imgui.h"

#include <iostream>

namespace mtcad {

std::unique_ptr<ToolOptions> RectangleTool::CreateOptions() const {
    return std::make_unique<RectangleToolOptions>();
}

void RectangleTool::RenderOptions(ToolOptions* options) {
    RectangleToolOptions* o = dynamic_cast<RectangleToolOptions*>(options);
    if (!o) {
        return;
    }
    ImGui::InputFloat("Width", &o->width);
    ImGui::InputFloat("Height", &o->height);
}

void RectangleTool::Apply(ToolOptions* options) {
    RectangleToolOptions* o = dynamic_cast<RectangleToolOptions*>(options);
    if (!o) return;
    std::cout << "Applying Rectangle tool: width=" << o->width << " height=" << o->height << "\n";
}

} // namespace mtcad