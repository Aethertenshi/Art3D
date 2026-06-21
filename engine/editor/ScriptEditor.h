#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "core/GameObject.h"
#include "scene/LogicScript.h"
#include "TextEditor.h"

struct OpenScriptInfo {
    LogicScript* ptr;
    std::string lastKnownName;
};

extern std::vector<OpenScriptInfo> g_openScripts;
extern std::unordered_map<LogicScript*, TextEditor> g_scriptEditors;
extern LogicScript* g_scriptToSelect;
extern bool g_showScriptEditor;
extern GameObject* g_lastSelectedObject;

void SyncOpenScripts(const std::vector<std::shared_ptr<GameObject>>& sceneObjects);
void DrawScriptEditor(GameObject*& selectedObject);
