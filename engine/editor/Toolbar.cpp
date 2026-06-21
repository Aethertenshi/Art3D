#include "Toolbar.h"
#include "imgui.h"
#include <string>

#define ICON_DRAG     "\ue1e6"
#define ICON_MOVE     "\ue121"
#define ICON_ROTATE   "\ue149"
#define ICON_SCALE    "\ue212"

namespace Toolbar {

void Draw(GizmoTool& currentTool) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + 30;
    const float windowWidth = viewport->WorkSize.x;

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, toolbarHeight), ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags toolbarFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.10f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(20, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 0));

    if (ImGui::Begin("##Toolbar", nullptr, toolbarFlags)) {
        ImGuiStyle& style = ImGui::GetStyle();
        float totalWidth = 0;
        const char* icons[] = { ICON_DRAG, ICON_MOVE, ICON_ROTATE, ICON_SCALE };
        for (const char* icon : icons) {
            totalWidth += ImGui::CalcTextSize(icon).x + style.FramePadding.x * 2;
        }
        totalWidth += style.ItemSpacing.x * (IM_ARRAYSIZE(icons) - 1);

        float offset = (windowWidth - totalWidth) * 0.5f - style.WindowPadding.x;
        if (offset > 0) ImGui::SetCursorPosX(offset);

        const char* tooltips[] = { "Drag", "Move", "Rotate", "Scale" };
        const char* shortcuts[] = { "1", "2", "3", "4" };

        for (int i = 0; i < 4; i++) {
            bool active = (currentTool == (GizmoTool)i);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            }
            if (ImGui::Button(icons[i], ImVec2(58, 58))) {
                currentTool = (GizmoTool)i;
            }
            if (active) ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                std::string tip = tooltips[i];
                tip += " ("; tip += shortcuts[i]; tip += ")";
                ImGui::SetTooltip("%s", tip.c_str());
            }
            if (i < 3) ImGui::SameLine(0, 0);
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

} // namespace Toolbar
