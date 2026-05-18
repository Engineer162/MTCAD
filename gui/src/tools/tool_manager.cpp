#include "tool_manager.h"

namespace mtcad {

ToolManager& ToolManager::Instance() {
    static ToolManager inst;
    return inst;
}

void ToolManager::RegisterTool(std::unique_ptr<Tool> tool) {
    if (!tool) return;
    tools_.emplace(static_cast<int>(tool->Id()), std::move(tool));
}

Tool* ToolManager::GetTool(ToolId id) {
    auto it = tools_.find(static_cast<int>(id));
    return it != tools_.end() ? it->second.get() : nullptr;
}

void ToolManager::OpenTool(ToolId id) {
    Tool* t = GetTool(id);
    if (!t) return;
    active_tool_id_ = id;
    active_options_ = t->CreateOptions();
}

void ToolManager::CloseActiveTool() {
    if (active_tool_id_ == ToolId::None) return;
    active_tool_id_ = ToolId::None;
    active_options_.reset();
}

Tool* ToolManager::ActiveTool() {
    return GetTool(active_tool_id_);
}

ToolOptions* ToolManager::ActiveOptions() {
    return active_options_.get();
}

bool ToolManager::IsToolOpen() const {
    return active_tool_id_ != ToolId::None && active_options_ != nullptr;
}

} // namespace mtcad
