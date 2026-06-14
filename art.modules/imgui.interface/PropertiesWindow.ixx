module;
#include "imgui.h"
#include <string.h>

export module PropertiesWindow;

import GameObject;

export class PropertiesWindow
{
public:
    void Draw(GameObject* selectedObject) {
        // Set default position and size (snapped to the left)
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 720), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Object Properties")) {
            if (selectedObject) {
                ImGui::Text("Selected: %s", selectedObject->Name.c_str());
                ImGui::Separator();

                // Edit Name
                char nameBuf[128];
                strncpy_s(nameBuf, selectedObject->Name.c_str(), sizeof(nameBuf));
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    selectedObject->Name = nameBuf;
                }

                ImGui::Spacing();
                ImGui::Text("Position2D");
                ImGui::Indent();
                ImGui::DragInt("Scale X", &selectedObject->Position.ScaleX, 1);
                ImGui::DragInt("Scale Y", &selectedObject->Position.ScaleY, 1);
                ImGui::DragInt("Offset X", &selectedObject->Position.OffsetX, 1);
                ImGui::DragInt("Offset Y", &selectedObject->Position.OffsetY, 1);
                ImGui::Unindent();

                ImGui::Spacing();
                ImGui::Text("Size2D");
                ImGui::Indent();
                ImGui::DragInt("Width Scale X", &selectedObject->Size.ScaleX, 1);
                ImGui::DragInt("Width Scale Y", &selectedObject->Size.ScaleY, 1);
                ImGui::DragInt("Width Offset X", &selectedObject->Size.OffsetX, 1);
                ImGui::DragInt("Width Offset Y", &selectedObject->Size.OffsetY, 1);
                ImGui::Unindent();

            } else {
                ImGui::Text("Select an object in the Hierarchy Explorer to view its properties.");
            }
        }
        ImGui::End();
    }
};
