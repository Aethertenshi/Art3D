#pragma once
#include "imgui.h"
#include <string.h>
#include <string>
#include "core/GameObject.h"
#include "scene/LightSource.h"
#include "scene/Sun.h"

class PropertiesWindow
{
public:
    void Draw(GameObject* selectedObject, bool& outSaveHistory) {
        if (ImGui::Begin("Properties")) {
            if (selectedObject) {
                ImGui::Text("Selected: %s", selectedObject->Name.c_str());
                ImGui::Separator();

                char nameBuf[128];
                strncpy_s(nameBuf, selectedObject->Name.c_str(), sizeof(nameBuf));
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputTextWithHint("##description", "Unnamed Object", nameBuf, sizeof(nameBuf))) {
                    selectedObject->Name = nameBuf;
                }
                if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                auto AxisDragF = [&](const char* label, const char* hiddenId, float* val, float speed, float min = -FLT_MAX, float max = FLT_MAX) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (min > -FLT_MAX && max < FLT_MAX) {
                        ImGui::DragFloat(hiddenId, val, speed, min, max);
                    } else {
                        ImGui::DragFloat(hiddenId, val, speed);
                    }
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                };

                auto AxisDragI = [&](const char* label, const char* hiddenId, int* val, float speed) {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(label);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragInt(hiddenId, val, speed);
                    if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                };

                auto BeginCard = [&](const char* id) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.15f, 1.00f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
                    ImGui::BeginChild(id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
                };

                auto EndCard = [&]() {
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                };

                if (selectedObject->Is3D) {
                    auto& t = *selectedObject;

                    BeginCard("Position");
                    ImGui::TextUnformatted("Position");
                    ImGui::Separator();
                    AxisDragF("X", "##posX", &t.Position3D.X, 0.05f);
                    AxisDragF("Y", "##posY", &t.Position3D.Y, 0.05f);
                    AxisDragF("Z", "##posZ", &t.Position3D.Z, 0.05f);
                    EndCard();

                    BeginCard("Rotation");
                    ImGui::TextUnformatted("Rotation");
                    ImGui::Separator();
                    AxisDragF("X", "##rotX", &t.Rotation3D.X, 1.0f);
                    AxisDragF("Y", "##rotY", &t.Rotation3D.Y, 1.0f);
                    AxisDragF("Z", "##rotZ", &t.Rotation3D.Z, 1.0f);
                    EndCard();

                    BeginCard("Scale");
                    ImGui::TextUnformatted("Scale");
                    ImGui::Separator();
                    AxisDragF("X", "##sclX", &t.Scale3D.X, 0.05f, 0.01f, 100.0f);
                    AxisDragF("Y", "##sclY", &t.Scale3D.Y, 0.05f, 0.01f, 100.0f);
                    AxisDragF("Z", "##sclZ", &t.Scale3D.Z, 0.05f, 0.01f, 100.0f);
                    EndCard();

                    BeginCard("MeshOffset");
                    ImGui::TextUnformatted("Mesh Offset");
                    ImGui::Separator();
                    AxisDragF("X", "##offX", &t.PositionOffset.X, 0.05f);
                    AxisDragF("Y", "##offY", &t.PositionOffset.Y, 0.05f);
                    AxisDragF("Z", "##offZ", &t.PositionOffset.Z, 0.05f);
                    EndCard();

                    BeginCard("MeshRot");
                    ImGui::TextUnformatted("Mesh Rotation");
                    ImGui::Separator();
                    AxisDragF("X", "##mrotX", &t.RotationOffset.X, 1.0f);
                    AxisDragF("Y", "##mrotY", &t.RotationOffset.Y, 1.0f);
                    AxisDragF("Z", "##mrotZ", &t.RotationOffset.Z, 1.0f);
                    EndCard();

                    Vector3D worldPos = t.GetWorldPosition();
                    BeginCard("WorldPos");
                    ImGui::TextUnformatted("World Position");
                    ImGui::Separator();
                    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("X"); ImGui::SameLine(); ImGui::Text("%.3f", worldPos.X);
                    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Y"); ImGui::SameLine(); ImGui::Text("%.3f", worldPos.Y);
                    ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Z"); ImGui::SameLine(); ImGui::Text("%.3f", worldPos.Z);
                    EndCard();

                    if (t.ClassName == "LightSource") {
                        LightSource* light = static_cast<LightSource*>(&t);
                        BeginCard("Light");
                        ImGui::TextUnformatted("Light");
                        ImGui::Separator();

                        const char* lightTypes[] = { "Point", "Spot", "Directional" };
                        int currentType = (int)light->Type;
                        if (ImGui::Combo("Type", &currentType, lightTypes, 3)) {
                            light->Type = (LightType)currentType;
                        }
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                        float colorArr[3] = { light->Color.X, light->Color.Y, light->Color.Z };
                        if (ImGui::ColorEdit3("Color", colorArr)) {
                            light->Color.X = colorArr[0];
                            light->Color.Y = colorArr[1];
                            light->Color.Z = colorArr[2];
                        }
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::DragFloat("Intensity", &light->Intensity, 0.05f, 0.0f, 100.0f);
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        ImGui::DragFloat("Range", &light->Range, 0.5f, 0.1f, 1000.0f);
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                        if (light->Type == LightType::Spot || light->Type == LightType::Directional) {
                            AxisDragF("Dir X", "##ldx", &light->Direction.X, 0.05f);
                            AxisDragF("Dir Y", "##ldy", &light->Direction.Y, 0.05f);
                            AxisDragF("Dir Z", "##ldz", &light->Direction.Z, 0.05f);
                        }
                        if (light->Type == LightType::Spot) {
                            ImGui::DragFloat("Inner Angle", &light->SpotAngleInner, 0.5f, 0.0f, light->SpotAngleOuter);
                            if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                            ImGui::DragFloat("Outer Angle", &light->SpotAngleOuter, 0.5f, light->SpotAngleInner, 180.0f);
                            if (ImGui::IsItemActivated()) { outSaveHistory = true; }
                        }

                        ImGui::Checkbox("Cast Shadow", &light->CastShadow);
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                        EndCard();
                    }

                    if (t.ClassName == "Sun") {
                        Sun* sun = static_cast<Sun*>(&t);
                        BeginCard("Sun");
                        ImGui::TextUnformatted("Sun Controls");
                        ImGui::Separator();

                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderFloat("Time of Day", &sun->TimeOfDay, 0.0f, 24.0f, "%.1f h")) {
                            sun->UpdateFromTime();
                        }
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                        int hour = (int)sun->TimeOfDay;
                        int minute = (int)((sun->TimeOfDay - hour) * 60);
                        ImGui::TextDisabled("  %02d:%02d", hour, minute);

                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::SliderFloat("Sun Sensitivity", &sun->SunSensitivity, 0.0f, 3.0f, "%.2f")) {
                            sun->UpdateFromTime();
                        }
                        if (ImGui::IsItemActivated()) { outSaveHistory = true; }

                        ImGui::Separator();
                        ImGui::TextDisabled("Direction: (%.2f, %.2f, %.2f)", sun->Direction.X, sun->Direction.Y, sun->Direction.Z);
                        ImGui::TextDisabled("Intensity: %.2f", sun->Intensity);

                        EndCard();
                    }
                } else {
                    auto& t = *selectedObject;

                    BeginCard("Position2D");
                    ImGui::TextUnformatted("Position");
                    ImGui::Separator();
                    AxisDragI("Scale X", "##psx", &t.Position.ScaleX, 1);
                    AxisDragI("Scale Y", "##psy", &t.Position.ScaleY, 1);
                    AxisDragI("Offset X", "##pox", &t.Position.OffsetX, 1);
                    AxisDragI("Offset Y", "##poy", &t.Position.OffsetY, 1);
                    EndCard();

                    BeginCard("Size2D");
                    ImGui::TextUnformatted("Size");
                    ImGui::Separator();
                    AxisDragI("Scale X", "##ssx", &t.Size.ScaleX, 1);
                    AxisDragI("Scale Y", "##ssy", &t.Size.ScaleY, 1);
                    AxisDragI("Offset X", "##sox", &t.Size.OffsetX, 1);
                    AxisDragI("Offset Y", "##soy", &t.Size.OffsetY, 1);
                    EndCard();
                }

            } else {
                ImGui::Text("Select an object.");
            }
        }
        ImGui::End();
    }
};
