#pragma once

namespace Toolbar {

enum GizmoTool { ToolDrag, ToolMove, ToolRotate, ToolScale };

void Draw(GizmoTool& currentTool);

} // namespace Toolbar
