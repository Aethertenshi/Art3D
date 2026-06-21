#include "ScriptEditor.h"
#include "Autocomplete.h"
#include "editor/ConsoleWindow.h"
#include "script/ScriptManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cctype>

std::vector<OpenScriptInfo> g_openScripts;
std::unordered_map<LogicScript*, TextEditor> g_scriptEditors;
LogicScript* g_scriptToSelect = nullptr;
bool g_showScriptEditor = false;
GameObject* g_lastSelectedObject = nullptr;

void SyncOpenScripts(const std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    std::unordered_set<LogicScript*> currentScripts;
    std::unordered_map<std::string, LogicScript*> currentScriptsByName;

    struct Helper {
        static void collect(const std::vector<std::shared_ptr<GameObject>>& objects,
                           std::unordered_set<LogicScript*>& outScripts,
                           std::unordered_map<std::string, LogicScript*>& outScriptsByName) {
            for (auto& obj : objects) {
                if (obj && obj->ClassName == "LogicScript") {
                    LogicScript* s = static_cast<LogicScript*>(obj.get());
                    outScripts.insert(s);
                    outScriptsByName[s->Name] = s;
                }
                if (obj) {
                    collect(obj->Children, outScripts, outScriptsByName);
                }
            }
        }
    };
    Helper::collect(sceneObjects, currentScripts, currentScriptsByName);

    std::vector<OpenScriptInfo> nextOpenScripts;
    std::unordered_set<LogicScript*> activePointers;

    for (auto& info : g_openScripts) {
        if (currentScripts.count(info.ptr) > 0) {
            info.lastKnownName = info.ptr->Name;
            nextOpenScripts.push_back(info);
            activePointers.insert(info.ptr);
        } else {
            auto it = currentScriptsByName.find(info.lastKnownName);
            if (it != currentScriptsByName.end()) {
                LogicScript* newPtr = it->second;
                nextOpenScripts.push_back({ newPtr, info.lastKnownName });
                activePointers.insert(newPtr);

                if (g_scriptEditors.count(info.ptr) > 0) {
                    g_scriptEditors[newPtr] = std::move(g_scriptEditors[info.ptr]);
                    g_scriptEditors.erase(info.ptr);
                }
                g_scriptEditors[newPtr].SetText(newPtr->SourceCode);
            }
        }
    }

    for (auto it = g_scriptEditors.begin(); it != g_scriptEditors.end(); ) {
        if (activePointers.count(it->first) == 0) {
            it = g_scriptEditors.erase(it);
        } else {
            ++it;
        }
    }

    if (g_scriptToSelect && currentScripts.count(g_scriptToSelect) == 0) {
        std::string lastKnown;
        for (const auto& oldInfo : g_openScripts) {
            if (oldInfo.ptr == g_scriptToSelect) { lastKnown = oldInfo.lastKnownName; break; }
        }
        if (!lastKnown.empty()) {
            auto it = currentScriptsByName.find(lastKnown);
            if (it != currentScriptsByName.end()) g_scriptToSelect = it->second;
            else g_scriptToSelect = nullptr;
        } else {
            g_scriptToSelect = nullptr;
        }
    }

    g_openScripts = std::move(nextOpenScripts);
}

extern ImFont* g_MonoFont;

void DrawScriptEditor(GameObject*& selectedObject) {
    if (selectedObject && selectedObject->ClassName == "LogicScript") {
        LogicScript* script = static_cast<LogicScript*>(selectedObject);
        bool found = false;
        for (const auto& info : g_openScripts) {
            if (info.ptr == script) { found = true; break; }
        }
        if (!found) {
            g_openScripts.push_back({ script, script->Name });
        }
        if (selectedObject != g_lastSelectedObject) {
            g_scriptToSelect = script;
            g_showScriptEditor = true;
            g_lastSelectedObject = selectedObject;
        }
    } else {
        g_showScriptEditor = false;
        g_lastSelectedObject = selectedObject;
    }

    if (!g_showScriptEditor || g_openScripts.empty()) {
        if (!g_showScriptEditor && selectedObject && selectedObject->ClassName == "LogicScript") {
            selectedObject = nullptr;
            g_lastSelectedObject = nullptr;
        }
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Script Editor", &g_showScriptEditor, ImGuiWindowFlags_MenuBar)) {
        static float scriptFontSize = 30.0f;
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
            if (io.MouseWheel != 0.0f) {
                scriptFontSize += io.MouseWheel * 1.0f;
                if (scriptFontSize < 8.0f) scriptFontSize = 8.0f;
                if (scriptFontSize > 72.0f) scriptFontSize = 72.0f;
                io.MouseWheel = 0.0f;
            }
        }

        if (ImGui::BeginMenuBar()) {
            ImGui::Text("(font size: %.0fpx), (ctrl + enter: quick debugging), artscript-editor", scriptFontSize);
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("ScriptTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
            LogicScript* scriptToClose = nullptr;

            for (size_t i = 0; i < g_openScripts.size(); i++) {
                LogicScript* script = g_openScripts[i].ptr;
                auto& editor = g_scriptEditors[script];

                if (editor.GetLanguageDefinition().mName != "JavaScript") {
                    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::JavaScript());
                    editor.SetText(script->SourceCode);
                    ScriptManager::RegisterScript(script->Name, script->SourceCode);
                }

                ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
                if (g_scriptToSelect == script) tabFlags |= ImGuiTabItemFlags_SetSelected;

                std::string tabLabel = script->Name.empty() ? "Unnamed Script" : script->Name;
                tabLabel += "##" + std::to_string((uintptr_t)script);

                bool open = true;
                if (ImGui::BeginTabItem(tabLabel.c_str(), &open, tabFlags)) {
                    if (selectedObject != script) {
                        selectedObject = script;
                        g_lastSelectedObject = script;
                    }

                    if (g_MonoFont) ImGui::PushFont(g_MonoFont, scriptFontSize);
                    editor.Render("##codeEditor");
                    if (g_MonoFont) ImGui::PopFont();

                    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                        ImGui::GetCurrentContext()->PlatformImeData.WantTextInput = true;
                    }

                    Autocomplete::ProcessAutocomplete(editor);

                    if (editor.IsTextChanged()) {
                        script->SourceCode = editor.GetText();
                    }

                    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                        ImGuiIO& io = ImGui::GetIO();
                        if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                            script->SourceCode = editor.GetText();
                            ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, "Executing script: %s", script->Name.c_str());
                            ScriptManager::Execute(script->Name, script->SourceCode);
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (g_scriptToSelect == script) g_scriptToSelect = nullptr;

                if (!open) scriptToClose = script;
            }

            if (scriptToClose) {
                ScriptManager::UnregisterScript(scriptToClose->Name);
                g_openScripts.erase(
                    std::remove_if(g_openScripts.begin(), g_openScripts.end(),
                        [scriptToClose](const OpenScriptInfo& info) { return info.ptr == scriptToClose; }),
                    g_openScripts.end()
                );
                g_scriptEditors.erase(scriptToClose);
                if (selectedObject == scriptToClose) {
                    if (!g_openScripts.empty()) {
                        selectedObject = g_openScripts.back().ptr;
                        g_scriptToSelect = g_openScripts.back().ptr;
                        g_lastSelectedObject = selectedObject;
                    } else {
                        selectedObject = nullptr;
                        g_lastSelectedObject = nullptr;
                        g_showScriptEditor = false;
                    }
                }
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
