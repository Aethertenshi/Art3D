#pragma once
#include "imgui.h"
#include <vector>
#include <memory>
#include <string>
#include "../art.datatypes/GameObject.h"

class ExplorerWindow
{
private:
    void DrawNode(GameObject* node, GameObject*& selectedObject) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (node->Children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selectedObject == node) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string label = node->Name;
        if (label.empty()) {
            label = "Unnamed Object";
        }
        label += "##" + std::to_string((uintptr_t)node);

        bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (ImGui::IsItemClicked()) {
            selectedObject = node;
        }

        if (open) {
            for (auto& child : node->Children) {
                DrawNode(child.get(), selectedObject);
            }
            ImGui::TreePop();
        }
    }

public:
    void Draw(const std::vector<std::shared_ptr<GameObject>>& rootObjects, GameObject*& selectedObject) {
        // Set default position and size (snapped to the right)
        ImGui::SetNextWindowPos(ImVec2(960, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320, 720), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Hierarchy Explorer")) {
            // Deselect if user left-clicks on empty space in the window
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
                selectedObject = nullptr;
            }

            for (auto& obj : rootObjects) {
                DrawNode(obj.get(), selectedObject);
            }
        }
        ImGui::End();
    }
};
