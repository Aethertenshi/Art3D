#pragma once
#include "imgui.h"
#include <string.h>
#include <string>
#include "../art.datatypes/GameObject.h"
#include "../art.objects/LightSource.h"

class PropertiesWindow
{
public:
    void Draw(GameObject* selectedObject, bool& outSaveHistory) {
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
                if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                if (selectedObject->Is3D) {
                    ImGui::Spacing();
                    ImGui::Text("Transform 3D (Local Space)");
                    ImGui::Indent();
                    if (ImGui::DragFloat("Position X", &selectedObject->Position3D.X, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Position Y", &selectedObject->Position3D.Y, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Position Z", &selectedObject->Position3D.Z, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Spacing();
                    if (ImGui::DragFloat("Rotation X (Deg)", &selectedObject->Rotation3D.X, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Rotation Y (Deg)", &selectedObject->Rotation3D.Y, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Rotation Z (Deg)", &selectedObject->Rotation3D.Z, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Spacing();
                    if (ImGui::DragFloat("Scale X", &selectedObject->Scale3D.X, 0.05f, 0.01f, 100.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Scale Y", &selectedObject->Scale3D.Y, 0.05f, 0.01f, 100.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Scale Z", &selectedObject->Scale3D.Z, 0.05f, 0.01f, 100.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Spacing();
                    ImGui::Text("Mesh Offset (Local Space)");
                    if (ImGui::DragFloat("Offset Pos X", &selectedObject->PositionOffset.X, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Offset Pos Y", &selectedObject->PositionOffset.Y, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Offset Pos Z", &selectedObject->PositionOffset.Z, 0.05f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Spacing();
                    if (ImGui::DragFloat("Offset Rot X (Deg)", &selectedObject->RotationOffset.X, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Offset Rot Y (Deg)", &selectedObject->RotationOffset.Y, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragFloat("Offset Rot Z (Deg)", &selectedObject->RotationOffset.Z, 1.0f)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Unindent();

                    Vector3D worldPos = selectedObject->GetWorldPosition();
                    ImGui::Spacing();
                    ImGui::Text("Transform 3D (World Space)");
                    ImGui::Indent();
                    ImGui::Text("Position X: %.3f", worldPos.X);
                    ImGui::Text("Position Y: %.3f", worldPos.Y);
                    ImGui::Text("Position Z: %.3f", worldPos.Z);
                    ImGui::Unindent();

                    if (selectedObject->ClassName == "LightSource") {
                        LightSource* light = static_cast<LightSource*>(selectedObject);
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Text("Light Source Properties");
                        ImGui::Indent();
                        float colorArr[3] = { light->Color.X, light->Color.Y, light->Color.Z };
                        if (ImGui::ColorEdit3("Light Color", colorArr)) {
                            light->Color.X = colorArr[0];
                            light->Color.Y = colorArr[1];
                            light->Color.Z = colorArr[2];
                        }
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                        if (ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f)) {}
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                        ImGui::Unindent();
                    }
                } else {
                    ImGui::Spacing();
                    ImGui::Text("Position2D");
                    ImGui::Indent();
                    if (ImGui::DragInt("Scale X", &selectedObject->Position.ScaleX, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Scale Y", &selectedObject->Position.ScaleY, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Offset X", &selectedObject->Position.OffsetX, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Offset Y", &selectedObject->Position.OffsetY, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Unindent();

                    ImGui::Spacing();
                    ImGui::Text("Size2D");
                    ImGui::Indent();
                    if (ImGui::DragInt("Width Scale X", &selectedObject->Size.ScaleX, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Width Scale Y", &selectedObject->Size.ScaleY, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Width Offset X", &selectedObject->Size.OffsetX, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    if (ImGui::DragInt("Width Offset Y", &selectedObject->Size.OffsetY, 1)) {}
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                    ImGui::Unindent();
                }

            } else {
                ImGui::Text("Select an object in the Hierarchy Explorer to view its properties.");
            }
        }
        ImGui::End();
    }
};
