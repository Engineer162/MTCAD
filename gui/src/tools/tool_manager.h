#pragma once
#include "sketch/create/tool.h"
#include <memory>
#include <unordered_map>

namespace mtcad {

class ToolManager {
public:
    static ToolManager& Instance();

    void RegisterTool(std::unique_ptr<Tool> tool);
    Tool* GetTool(ToolId id);

    // Open a tool's options window
    void OpenTool(ToolId id);
    void CloseActiveTool();

    Tool* ActiveTool();
    ToolOptions* ActiveOptions();
    bool IsToolOpen() const;

private:
    std::unordered_map<int, std::unique_ptr<Tool>> tools_;
    ToolId active_tool_id_ = ToolId::None;
    std::unique_ptr<ToolOptions> active_options_;
};

} // namespace mtcad
