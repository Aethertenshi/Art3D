#pragma once
#include <SDL3/SDL.h>
#include "imgui.h"
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <functional>

#define ICON_FILE          "\ue0c0"
#define ICON_FILE_IMAGE    "\ue31c"
#define ICON_FILE_AUDIO    "\ue31a"
#define ICON_FILE_CODE     "\ue0c3"
#define ICON_FILE_TEXT     "\ue0cc"
#define ICON_FILE_MODEL    "\ue30e"
#define ICON_FOLDER        "\ue0d7"
#define ICON_FOLDER_OPEN   "\ue247"

class AssetBrowser {
public:
    enum class AssetType {
        Unknown,
        Folder,
        Model,
        Image,
        Audio,
        Script,
        Font,
        Shader
    };

    struct AssetEntry {
        std::string name;
        std::string fullPath;
        std::string extension;
        AssetType type;
    };

    AssetBrowser() {
        const char* base = SDL_GetBasePath();
        m_RootPath = base ? base : ".";
        m_CurrentPath = m_RootPath;
        ScanDirectory();
    }

    // Callback when a non-folder file is activated (double-clicked)
    std::function<void(const std::string&)> m_OnActivateFile;

    void SetRootPath(const std::string& path) {
        if (std::filesystem::exists(path)) {
            m_RootPath = path;
            m_CurrentPath = path;
            ScanDirectory();
        }
    }

    void Draw() {
        if (!ImGui::Begin("Asset Browser")) {
            ImGui::End();
            return;
        }

        DrawBreadcrumbBar();
        ImGui::Separator();
        DrawGrid();
        ImGui::End();
    }

private:
    std::string m_RootPath;
    std::string m_CurrentPath;
    std::vector<AssetEntry> m_Entries;
    std::string m_SelectedPath;

    static AssetType GetAssetType(const std::string& ext) {
        std::string lower = ext;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == ".obj" || lower == ".gltf" || lower == ".glb")
            return AssetType::Model;
        if (lower == ".png" || lower == ".jpg" || lower == ".jpeg" ||
            lower == ".bmp" || lower == ".tga" || lower == ".psd" ||
            lower == ".hdr" || lower == ".gif" || lower == ".tiff" || lower == ".webp")
            return AssetType::Image;
        if (lower == ".wav" || lower == ".mp3" || lower == ".ogg" ||
            lower == ".flac" || lower == ".aac" || lower == ".wma" || lower == ".m4a")
            return AssetType::Audio;
        if (lower == ".js" || lower == ".txt" || lower == ".lua" ||
            lower == ".py" || lower == ".json" || lower == ".xml")
            return AssetType::Script;
        if (lower == ".ttf" || lower == ".otf" || lower == ".woff" ||
            lower == ".woff2" || lower == ".fon")
            return AssetType::Font;
        if (lower == ".hlsl" || lower == ".fx" || lower == ".shader" ||
            lower == ".glsl" || lower == ".vs" || lower == ".ps" || lower == ".dxil")
            return AssetType::Shader;
        return AssetType::Unknown;
    }

    static bool IsSupported(const std::string& ext) {
        return GetAssetType(ext) != AssetType::Unknown;
    }

    static const char* GetIconForType(AssetType type, bool isFolder = false, bool isOpen = false) {
        if (isFolder) return isOpen ? ICON_FOLDER_OPEN : ICON_FOLDER;
        switch (type) {
            case AssetType::Model:  return ICON_FILE_MODEL;
            case AssetType::Image:  return ICON_FILE_IMAGE;
            case AssetType::Audio:  return ICON_FILE_AUDIO;
            case AssetType::Script: return ICON_FILE_CODE;
            case AssetType::Font:   return ICON_FILE_TEXT;
            case AssetType::Shader: return ICON_FILE_CODE;
            default:                return ICON_FILE;
        }
    }

    static void TruncateName(std::string& name, int maxLen) {
        if ((int)name.size() > maxLen) {
            name = name.substr(0, maxLen - 3) + "...";
        }
    }

    void ScanDirectory() {
        m_Entries.clear();

        if (!std::filesystem::exists(m_CurrentPath))
            return;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(m_CurrentPath)) {
                AssetEntry ae;
                ae.fullPath = entry.path().string();
                ae.name = entry.path().filename().string();

                if (entry.is_directory()) {
                    if (ae.name[0] != '.') {
                        ae.type = AssetType::Folder;
                        ae.extension = "";
                        m_Entries.push_back(ae);
                    }
                    continue;
                }

                ae.extension = entry.path().extension().string();
                std::transform(ae.extension.begin(), ae.extension.end(), ae.extension.begin(), ::tolower);

                if (!IsSupported(ae.extension))
                    continue;

                ae.type = GetAssetType(ae.extension);
                m_Entries.push_back(ae);
            }
        } catch (const std::exception& e) {
            std::cerr << "AssetBrowser scan error: " << e.what() << std::endl;
        }

        std::sort(m_Entries.begin(), m_Entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
            if (a.type == AssetType::Folder && b.type != AssetType::Folder) return true;
            if (a.type != AssetType::Folder && b.type == AssetType::Folder) return false;
            std::string al = a.name, bl = b.name;
            std::transform(al.begin(), al.end(), al.begin(), ::tolower);
            std::transform(bl.begin(), bl.end(), bl.begin(), ::tolower);
            return al < bl;
        });
    }

    void DrawBreadcrumbBar() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

        // "Root" button to go to project root
        if (ImGui::SmallButton(ICON_FOLDER " Root")) {
            m_CurrentPath = m_RootPath;
            ScanDirectory();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(">");
        ImGui::SameLine();

        // Breadcrumb: show path components as clickable buttons
        std::string relPath = m_CurrentPath;
        std::string display;
        if (relPath.size() > 50) {
            display = "..." + relPath.substr(relPath.size() - 47);
        } else {
            display = relPath;
        }
        ImGui::TextUnformatted(display.c_str());

        // "Up" button
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x - 120, 0));
        ImGui::SameLine();
        if (ImGui::SmallButton("Up")) {
            std::filesystem::path parent = std::filesystem::path(m_CurrentPath).parent_path();
            if (parent != m_CurrentPath && std::filesystem::exists(parent)) {
                m_CurrentPath = parent.string();
                ScanDirectory();
            }
        }

        // "..." browse button to pick a new root directory
        ImGui::SameLine();
        if (ImGui::SmallButton("...")) {
            // Open an ImGui input dialog to manually set path
            ImGui::OpenPopup("Set Asset Path");
        }

        ImGui::PopStyleVar();

        // Path input popup
        if (ImGui::BeginPopup("Set Asset Path")) {
            static char pathBuf[1024] = "";
            ImGui::InputText("Path", pathBuf, sizeof(pathBuf));
            ImGui::SameLine();
            if (ImGui::Button("Set")) {
                if (std::filesystem::exists(pathBuf)) {
                    m_RootPath = pathBuf;
                    m_CurrentPath = pathBuf;
                    ScanDirectory();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawGrid() {
        ImVec2 cellSize(90, 100);
        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columns = (int)(panelWidth / (cellSize.x + ImGui::GetStyle().ItemSpacing.x));
        if (columns < 1) columns = 1;

        ImGui::Columns(columns, "AssetGrid", false);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));

        for (int i = 0; i < (int)m_Entries.size(); i++) {
            const auto& entry = m_Entries[i];

            // Grid cell with border
            ImGui::PushID(i);

            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Background highlight for selected
            bool isSelected = (m_SelectedPath == entry.fullPath);

            // Draw icon area (rectangle with icon centered)
            ImVec2 iconSize(70, 60);
            ImVec2 iconMin(cursorPos.x + (cellSize.x - iconSize.x) * 0.5f, cursorPos.y);
            ImVec2 iconMax(iconMin.x + iconSize.x, iconMin.y + iconSize.y);

            // Background rect
            ImU32 bgColor = isSelected ? IM_COL32(60, 100, 180, 50) : IM_COL32(40, 40, 50, 30);
            drawList->AddRectFilled(iconMin, iconMax, bgColor, 4.0f);
            if (isSelected) {
                drawList->AddRect(iconMin, iconMax, IM_COL32(100, 150, 255, 200), 4.0f);
            }

            // Icon text centered in the icon area
            const char* icon = GetIconForType(entry.type, entry.type == AssetType::Folder, false);
            ImVec2 textSize = ImGui::CalcTextSize(icon);
            drawList->AddText(ImVec2(iconMin.x + (iconSize.x - textSize.x) * 0.5f,
                                     iconMin.y + (iconSize.y - textSize.y) * 0.5f),
                              IM_COL32_WHITE, icon);

            // Name below icon, truncated
            std::string displayName = entry.name;
            TruncateName(displayName, 14);
            ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());

            // Invisible selectable covering the whole cell
            ImGui::SetCursorScreenPos(cursorPos);
            ImGui::InvisibleButton("cell", cellSize);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (entry.type == AssetType::Folder) {
                    m_CurrentPath = entry.fullPath;
                    ScanDirectory();
                } else if (m_OnActivateFile) {
                    m_OnActivateFile(entry.fullPath);
                }
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_SelectedPath = entry.fullPath;
            }

            // Drag source for all entries (including folders for navigation? no, just for files)
            if (entry.type != AssetType::Folder) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("ASSET_PATH", entry.fullPath.c_str(), entry.fullPath.size() + 1);
                    ImGui::Text("%s %s", GetIconForType(entry.type), entry.name.c_str());
                    ImGui::EndDragDropSource();
                }
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem("AssetContext")) {
                if (ImGui::MenuItem("Copy Path")) {
                    ImGui::SetClipboardText(entry.fullPath.c_str());
                }
                if (entry.type == AssetType::Folder) {
                    if (ImGui::MenuItem("Set as Root")) {
                        m_RootPath = entry.fullPath;
                    }
                    if (ImGui::MenuItem("Open in Explorer")) {
                        std::string cmd = "explorer /select,\"" + entry.fullPath + "\"";
                        system(cmd.c_str());
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();

            // Draw filename text centered below the icon
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, iconMax.y + 2));
            ImGui::SetCursorPosX(cursorPos.x + (cellSize.x - nameSize.x) * 0.5f);
            ImGui::TextUnformatted(displayName.c_str());

            ImGui::NextColumn();
        }

        ImGui::PopStyleVar();
        ImGui::Columns(1);
    }

};
