#pragma once
#include "imgui.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <functional>
#include "core/GameObject.h"

#define ICON_CUBOID     "\ue524"
#define ICON_SUN        "\ue178"
#define ICON_FILE_CODE  "\ue0c3"
#define ICON_LAYOUT     "\ue12c"

class ExplorerWindow
{
private:
    std::function<void(GameObject*, const std::string&)> m_InsertCallback;

    static const char* GetIconFor(GameObject* node) {
        if (node->ClassName == "Part" || node->ClassName == "BasePart")
            return ICON_CUBOID;
        if (node->ClassName == "LightSource")
            return ICON_SUN;
        if (node->ClassName == "Sun")
            return ICON_SUN;
        if (node->ClassName == "LogicScript")
            return ICON_FILE_CODE;
        if (node->Is3D)
            return ICON_CUBOID;
        return ICON_LAYOUT;
    }

    void DrawNode(GameObject* node, GameObject*& selectedObject) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (node->Children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selectedObject == node) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string label = GetIconFor(node);
        if (!node->Name.empty()) {
            label += "  " + node->Name;
        } else {
            label += "  Unnamed Object";
        }
        label += "##" + std::to_string((uintptr_t)node);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

        bool open = ImGui::TreeNodeEx(label.c_str(), flags);

        ImGui::PopStyleVar();

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            selectedObject = node;
        }

        if (ImGui::BeginPopupContextItem()) {
            if (selectedObject != node) selectedObject = node;
            ImGui::Text("Insert into %s", node->Name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_CUBOID " 3D Object")) {
                if (m_InsertCallback) m_InsertCallback(node, "3D");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(ICON_SUN " Light Source")) {
                if (m_InsertCallback) m_InsertCallback(node, "Light");
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem(ICON_FILE_CODE " Logic Script")) {
                if (m_InsertCallback) m_InsertCallback(node, "Script");
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

        if (open) {
            for (auto& child : node->Children) {
                DrawNode(child.get(), selectedObject);
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleVar();
    }

public:
    void SetInsertCallback(std::function<void(GameObject*, const std::string&)> callback) {
        m_InsertCallback = std::move(callback);
    }

    void Draw(const std::vector<std::shared_ptr<GameObject>>& rootObjects, GameObject*& selectedObject) {
        if (ImGui::Begin("Explorer")) {
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
                selectedObject = nullptr;
            }

            static char text[128] = "";
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##description", "Search any Objects...", text, IM_ARRAYSIZE(text));

            std::string filterText(text);
            std::transform(filterText.begin(), filterText.end(), filterText.begin(), ::tolower);

            for (auto& obj : rootObjects) {
                if (filterText.empty()) {
                    DrawNode(obj.get(), selectedObject);
                    continue;
                }

                std::string objName = obj->Name;
                std::transform(objName.begin(), objName.end(), objName.begin(), ::tolower);

                if (objName.find(filterText) != std::string::npos) {
                    DrawNode(obj.get(), selectedObject);
                }
            }

            if (selectedObject && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_I)) {
                ImGui::OpenPopup("##CtrlInsertMenu");
            }

            if (ImGui::BeginPopup("##CtrlInsertMenu")) {
                ImGui::Text("Insert into %s", selectedObject ? selectedObject->Name.c_str() : "...");
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_CUBOID " 3D Object")) {
                    if (m_InsertCallback) m_InsertCallback(selectedObject, "3D");
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::MenuItem(ICON_SUN " Light Source")) {
                    if (m_InsertCallback) m_InsertCallback(selectedObject, "Light");
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::MenuItem(ICON_FILE_CODE " Logic Script")) {
                    if (m_InsertCallback) m_InsertCallback(selectedObject, "Script");
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
};
