#include "tools_init.h"
#include "tool_manager.h"
#include "sketch/create/line_tool.h"
#include "sketch/create/rectangle_tool.h"
#include "sketch/create/circle_tool.h"

namespace mtcad {

void InitializeTools() {
    // Register built-in example tools
    ToolManager::Instance().RegisterTool(std::make_unique<LineTool>());
    ToolManager::Instance().RegisterTool(std::make_unique<RectangleTool>());
    ToolManager::Instance().RegisterTool(std::make_unique<CircleTool>());
}

} // namespace mtcad
