#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "TextEditor.h"

#include "art.modules/art.datatypes/Position2D.h"
#include "art.modules/art.datatypes/Size2D.h"
#include "art.modules/art.userinterface/BaseDrawables.h"
#include "art.modules/art.userinterface/Frame.h"
#include "art.modules/art.datatypes/GameObject.h"
#include "art.modules/imgui.interface/ExplorerWindow.h"
#include "art.modules/imgui.interface/PropertiesWindow.h"
#include "art.modules/imgui.interface/ConsoleWindow.h"
#include "art.modules/art.userinterface/GpuPipeline.h"
#include "art.modules/art.datatypes/Matrix4x4.h"
#include "art.modules/art.enginemovement/CameraController.h"
#include "art.modules/art.datatypes/Vector3D.h"
#include "art.modules/art.objects/LightSource.h"
#include "art.modules/art.objects/Part.h"
#include "art.modules/art.objects/LogicScript.h"
#include "art.modules/art.script-logic/ScriptManager.h"

// Lucide icon codepoints (Private Use Area U+E000..U+EFFF)
#define ICON_DRAG     "\ue1e6"   // grab (drag tool)
#define ICON_MOVE     "\ue121"   // move (translate tool)
#define ICON_ROTATE   "\ue149"   // rotate-cw (rotate tool)
#define ICON_SCALE    "\ue212"   // scale tool
#define ICON_BOX      "\ue061"   // box
#define ICON_IMPORT   "\ue22f"   // import
#define ICON_X        "\ue084"   // x/cancel

// Global mono font reference for code/log/script panels (set during ImGui init)
ImFont* g_MonoFont = nullptr;

// Autocomplete state variables
static bool g_autocompleteOpen = false;
static bool g_autocompleteDismissed = false;
static bool g_resetDockspaceLayout = false;
static int g_autocompleteActiveIndex = 0;
static bool g_autocompleteSelectTriggered = false;
static bool g_autocompleteCloseTriggered = false;
static std::vector<std::string> g_autocompleteMatches;
static const std::vector<std::string> combinedKeywordsList = {
    "arguments", "as", "async", "await", "break", "case", "catch", 
    "class", "const", "constructor", "continue", "debugger", "default", 
    "delete", "do", "else", "enum", "error", "export", "extends", 
    "false", "finally", "for", "foreign", "from", "function", "get", 
    "getters", "if", "import", "in", "instanceof", "is", "let", 
    "new", "null", "of", "print", "return", "set", "setters", 
    "static", "super", "switch", "target", "this", "throw", "true", 
    "try", "typeof", "undefined", "var", "void", "warn", "while", 
    "with", "yield"
};

// Script Editor tab state
struct OpenScriptInfo {
    LogicScript* ptr;
    std::string lastKnownName;
};

static std::vector<OpenScriptInfo> g_openScripts;
static std::unordered_map<LogicScript*, TextEditor> g_scriptEditors;
static LogicScript* g_scriptToSelect = nullptr;
static bool g_showScriptEditor = false;
static GameObject* g_lastSelectedObject = nullptr;

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
    
    // Prune any editors that are no longer active
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
            if (oldInfo.ptr == g_scriptToSelect) {
                lastKnown = oldInfo.lastKnownName;
                break;
            }
        }
        if (!lastKnown.empty()) {
            auto it = currentScriptsByName.find(lastKnown);
            if (it != currentScriptsByName.end()) {
                g_scriptToSelect = it->second;
            } else {
                g_scriptToSelect = nullptr;
            }
        } else {
            g_scriptToSelect = nullptr;
        }
    }

    g_openScripts = std::move(nextOpenScripts);
}

std::string GetCurrentWordPrefix(const TextEditor& editor) {
    TextEditor::Coordinates cursor = editor.GetCursorPosition();
    TextEditor::Coordinates lineStart(cursor.mLine, 0);
    std::string lineToCursor = editor.GetText(lineStart, cursor);

    int len = (int)lineToCursor.length();
    int i = len - 1;
    while (i >= 0) {
        char c = lineToCursor[i];
        if (std::isalnum((unsigned char)c) || c == '_') {
            i--;
        } else {
            break;
        }
    }
    return lineToCursor.substr(i + 1);
}

void InsertSuggestion(TextEditor& editor, const std::string& suggestion) {
    TextEditor::Coordinates cursor = editor.GetCursorPosition();
    std::string prefix = GetCurrentWordPrefix(editor);
    
    int cursorCharIndex = editor.GetCharacterIndex(cursor);
    int startCharIndex = cursorCharIndex - (int)prefix.length();
    int startColumn = editor.GetCharacterColumn(cursor.mLine, startCharIndex);
    
    TextEditor::Coordinates start(cursor.mLine, startColumn);
    editor.DeleteRange(start, cursor);
    TextEditor::Coordinates newCursor = start;
    editor.InsertTextAt(newCursor, suggestion.c_str());
    editor.SetCursorPosition(newCursor);
}

void Render2DObject(GameObject* obj) {
    if (!obj) return;
    if (!obj->Is3D) {
        if (obj->ClassName == "Frame" || obj->ClassName == "BaseDrawables") {
            BaseDrawables* drawable = static_cast<BaseDrawables*>(obj);
            drawable->Draw();
        } else if (obj->ClassName == "GameObject") {
            // Draw a light gray semi-transparent rectangle for generic 2D GameObjects
            ImGui::GetForegroundDrawList()->AddRectFilled(
                ImVec2((float)obj->Position.OffsetX, (float)obj->Position.OffsetY),
                ImVec2((float)(obj->Position.OffsetX + obj->Size.OffsetX), (float)(obj->Position.OffsetY + obj->Size.OffsetY)),
                IM_COL32(100, 100, 100, 50)
            );
            ImGui::GetForegroundDrawList()->AddRect(
                ImVec2((float)obj->Position.OffsetX, (float)obj->Position.OffsetY),
                ImVec2((float)(obj->Position.OffsetX + obj->Size.OffsetX), (float)(obj->Position.OffsetY + obj->Size.OffsetY)),
                IM_COL32(150, 150, 150, 255)
            );
        }
    }
    // Render children recursively
    for (auto& child : obj->Children) {
        Render2DObject(child.get());
    }
}

struct GPULight {
    float pos[4];        // .xyz = pos, .w = intensity
    float dir[4];        // .xyz = direction, .w = range
    float color[4];      // .xyz = color, .w = type (0=point,1=spot,2=dir)
    float spotAngles[2]; // .x = inner cos, .y = outer cos
    float pad[2];
};

struct LightUBO {
    GPULight lights[8];
    int lightCount;
    float ambientIntensity;
};

bool RayAABBIntersect(
    const Vector3D& rayOrigin, const Vector3D& rayDir, float rayLength,
    const Vector3D& boxCenter, const Vector3D& boxHalfSize,
    float& tNear, float& tFar
) {
    float tMin = -1e9f;
    float tMax = 1e9f;

    // X axis
    if (std::abs(rayDir.X) < 1e-6f) {
        if (rayOrigin.X < boxCenter.X - boxHalfSize.X || rayOrigin.X > boxCenter.X + boxHalfSize.X) {
            return false;
        }
    } else {
        float invD = 1.0f / rayDir.X;
        float t1 = (boxCenter.X - boxHalfSize.X - rayOrigin.X) * invD;
        float t2 = (boxCenter.X + boxHalfSize.X - rayOrigin.X) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    // Y axis
    if (std::abs(rayDir.Y) < 1e-6f) {
        if (rayOrigin.Y < boxCenter.Y - boxHalfSize.Y || rayOrigin.Y > boxCenter.Y + boxHalfSize.Y) {
            return false;
        }
    } else {
        float invD = 1.0f / rayDir.Y;
        float t1 = (boxCenter.Y - boxHalfSize.Y - rayOrigin.Y) * invD;
        float t2 = (boxCenter.Y + boxHalfSize.Y - rayOrigin.Y) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    // Z axis
    if (std::abs(rayDir.Z) < 1e-6f) {
        if (rayOrigin.Z < boxCenter.Z - boxHalfSize.Z || rayOrigin.Z > boxCenter.Z + boxHalfSize.Z) {
            return false;
        }
    } else {
        float invD = 1.0f / rayDir.Z;
        float t1 = (boxCenter.Z - boxHalfSize.Z - rayOrigin.Z) * invD;
        float t2 = (boxCenter.Z + boxHalfSize.Z - rayOrigin.Z) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    tNear = tMin;
    tFar = tMax;
    return true;
}

void Collect3DObjects(GameObject* obj, std::vector<GameObject*>& outList) {
    if (!obj) return;
    if (obj->Is3D) {
        outList.push_back(obj);
    }
    for (auto& child : obj->Children) {
        Collect3DObjects(child.get(), outList);
    }
}

Vector3D GetFinalWorldCenter(GameObject* obj) {
    if (!obj) return Vector3D(0, 0, 0);
    Vector3D worldPos = obj->GetWorldPosition();
    Vector3D worldRot = obj->GetWorldRotation();
    Vector3D worldScale = obj->GetWorldScale();

    float rotX = worldRot.X * 3.14159265f / 180.0f;
    float rotY = worldRot.Y * 3.14159265f / 180.0f;
    float rotZ = worldRot.Z * 3.14159265f / 180.0f;

    float offRotX = obj->RotationOffset.X * 3.14159265f / 180.0f;
    float offRotY = obj->RotationOffset.Y * 3.14159265f / 180.0f;
    float offRotZ = obj->RotationOffset.Z * 3.14159265f / 180.0f;

    float scaleX = worldScale.X;
    float scaleY = worldScale.Y;
    float scaleZ = worldScale.Z;
    if (obj->ClassName == "LightSource") {
        scaleX *= 0.15f; scaleY *= 0.15f; scaleZ *= 0.15f;
    }

    Matrix4x4 model = Matrix4x4::Translation(obj->PositionOffset.X, obj->PositionOffset.Y, obj->PositionOffset.Z)
        .Multiply(Matrix4x4::RotationZ(offRotZ))
        .Multiply(Matrix4x4::RotationX(offRotX))
        .Multiply(Matrix4x4::RotationY(offRotY))
        .Multiply(Matrix4x4::Scale(scaleX, scaleY, scaleZ))
        .Multiply(Matrix4x4::RotationZ(rotZ))
        .Multiply(Matrix4x4::RotationX(rotX))
        .Multiply(Matrix4x4::RotationY(rotY))
        .Multiply(Matrix4x4::Translation(worldPos.X, worldPos.Y, worldPos.Z));

    return Vector3D(model.m[3][0], model.m[3][1], model.m[3][2]);
}

bool IsObjectShadowed(GameObject* target, const std::vector<std::shared_ptr<GameObject>>& sceneObjects, const Vector3D& lightPos) {
    if (!target) return false;
    Vector3D targetPos = GetFinalWorldCenter(target);
    Vector3D rayDir = lightPos - targetPos;
    float rayLength = std::sqrt(rayDir.X * rayDir.X + rayDir.Y * rayDir.Y + rayDir.Z * rayDir.Z);
    if (rayLength < 0.001f) return false;

    // Normalize ray direction
    rayDir.X /= rayLength;
    rayDir.Y /= rayLength;
    rayDir.Z /= rayLength;

    // Collect all 3D objects in the scene
    std::vector<GameObject*> all3DObjects;
    for (const auto& root : sceneObjects) {
        Collect3DObjects(root.get(), all3DObjects);
    }

    // Check intersection with all other 3D objects
    for (GameObject* blocker : all3DObjects) {
        if (blocker == target) continue;
        if (blocker->ClassName == "LightSource") continue; // Light sources do not block light

        Vector3D boxPos = GetFinalWorldCenter(blocker);
        Vector3D boxScale = blocker->GetWorldScale();
        
        // Compute half size
        float hx = boxScale.X * 0.5f;
        float hy = boxScale.Y * 0.5f;
        float hz = boxScale.Z * 0.5f;

        float tNear = 0.0f;
        float tFar = 0.0f;
        if (RayAABBIntersect(targetPos, rayDir, rayLength, boxPos, Vector3D(hx, hy, hz), tNear, tFar)) {
            // Find the overlap between the ray bounds [0.05f, rayLength - 0.05f] and box entry/exit [tNear, tFar]
            float overlapStart = std::max(tNear, 0.05f);
            float overlapEnd = std::min(tFar, rayLength - 0.05f);
            if (overlapStart < overlapEnd) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::shared_ptr<GameObject>> CloneScene(const std::vector<std::shared_ptr<GameObject>>& scene) {
    std::vector<std::shared_ptr<GameObject>> clonedScene;
    for (const auto& obj : scene) {
        clonedScene.push_back(obj->Clone());
    }
    return clonedScene;
}

GameObject* FindObjectByName(GameObject* root, const std::string& name) {
    if (!root) return nullptr;
    if (root->Name == name) return root;
    for (auto& child : root->Children) {
        GameObject* found = FindObjectByName(child.get(), name);
        if (found) return found;
    }
    return nullptr;
}

GameObject* FindSelectedInScene(const std::vector<std::shared_ptr<GameObject>>& scene, const std::string& name) {
    for (auto& obj : scene) {
        GameObject* found = FindObjectByName(obj.get(), name);
        if (found) return found;
    }
    return nullptr;
}

bool RemoveObjectFromHierarchy(GameObject* parent, GameObject* target) {
    if (!parent) return false;
    for (auto it = parent->Children.begin(); it != parent->Children.end(); ++it) {
        if (it->get() == target) {
            parent->Children.erase(it);
            return true;
        }
        if (RemoveObjectFromHierarchy(it->get(), target)) {
            return true;
        }
    }
    return false;
}

void DeleteObject(GameObject* target, std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    if (!target) return;
    for (auto it = sceneObjects.begin(); it != sceneObjects.end(); ++it) {
        if (it->get() == target) {
            sceneObjects.erase(it);
            return;
        }
    }
    for (auto& root : sceneObjects) {
        if (RemoveObjectFromHierarchy(root.get(), target)) {
            return;
        }
    }
}

struct GpuBufferDeleter {
    SDL_GPUDevice* Device;
    void operator()(SDL_GPUBuffer* buffer) const {
        if (Device && buffer) {
            SDL_ReleaseGPUBuffer(Device, buffer);
        }
    }
};

bool LoadObj(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
        return false;
    }

    std::vector<Vector3D> temp_positions;
    std::vector<Vector3D> temp_normals;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_positions.push_back(Vector3D(x, y, z));
        } else if (prefix == "vn") {
            float nx, ny, nz;
            ss >> nx >> ny >> nz;
            temp_normals.push_back(Vector3D(nx, ny, nz));
        } else if (prefix == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token) {
                faceTokens.push_back(token);
            }

            if (faceTokens.size() < 3) continue;

            auto parseVertexToken = [&](const std::string& tok) -> Vertex {
                std::stringstream tokSS(tok);
                std::string vStr, vtStr, vnStr;
                std::getline(tokSS, vStr, '/');
                std::getline(tokSS, vtStr, '/');
                std::getline(tokSS, vnStr, '/');

                int vIdx = 0;
                try {
                    vIdx = std::stoi(vStr) - 1;
                } catch (...) {
                    vIdx = -1;
                }

                if (vIdx < 0 && !temp_positions.empty()) {
                    vIdx = (int)temp_positions.size() + vIdx + 1;
                }

                Vertex v = {};
                if (vIdx >= 0 && vIdx < (int)temp_positions.size()) {
                    v.x = temp_positions[vIdx].X;
                    v.y = temp_positions[vIdx].Y;
                    v.z = temp_positions[vIdx].Z;
                }
                
                v.r = 0.8f;
                v.g = 0.8f;
                v.b = 0.8f;
                v.a = 1.0f;

                int vnIdx = -1;
                if (!vnStr.empty()) {
                    try {
                        vnIdx = std::stoi(vnStr) - 1;
                    } catch (...) {
                        vnIdx = -1;
                    }
                }

                if (vnIdx >= 0 && vnIdx < (int)temp_normals.size()) {
                    v.nx = temp_normals[vnIdx].X;
                    v.ny = temp_normals[vnIdx].Y;
                    v.nz = temp_normals[vnIdx].Z;
                } else {
                    v.nx = 0.0f; v.ny = 0.0f; v.nz = 0.0f;
                }
                return v;
            };

            std::vector<Vertex> faceVertices;
            for (const auto& tok : faceTokens) {
                faceVertices.push_back(parseVertexToken(tok));
            }

            for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
                Vertex v0 = faceVertices[0];
                Vertex v1 = faceVertices[i + 1];
                Vertex v2 = faceVertices[i];

                if (v0.nx == 0.0f && v0.ny == 0.0f && v0.nz == 0.0f &&
                    v1.nx == 0.0f && v1.ny == 0.0f && v1.nz == 0.0f &&
                    v2.nx == 0.0f && v2.ny == 0.0f && v2.nz == 0.0f) {
                    
                    Vector3D edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
                    Vector3D edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
                    Vector3D norm(
                        edge1.Y * edge2.Z - edge1.Z * edge2.Y,
                        edge1.Z * edge2.X - edge1.X * edge2.Z,
                        edge1.X * edge2.Y - edge1.Y * edge2.X
                    );
                    float len = sqrtf(norm.X * norm.X + norm.Y * norm.Y + norm.Z * norm.Z);
                    if (len > 1e-6f) {
                        norm.X /= len; norm.Y /= len; norm.Z /= len;
                    } else {
                        norm = Vector3D(0.0f, 1.0f, 0.0f);
                    }
                    v0.nx = v1.nx = v2.nx = norm.X;
                    v0.ny = v1.ny = v2.ny = norm.Y;
                    v0.nz = v1.nz = v2.nz = norm.Z;
                }

                uint32_t startIdx = (uint32_t)outVertices.size();
                outVertices.push_back(v0);
                outVertices.push_back(v1);
                outVertices.push_back(v2);

                outIndices.push_back(startIdx);
                outIndices.push_back(startIdx + 1);
                outIndices.push_back(startIdx + 2);
            }
        }
    }

    if (outVertices.empty()) return false;

    // Calculate bounding box center
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;

    for (const auto& v : outVertices) {
        if (v.x < minX) minX = v.x;
        if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.y > maxY) maxY = v.y;
        if (v.z < minZ) minZ = v.z;
        if (v.z > maxZ) maxZ = v.z;
    }

    outCenter.X = (minX + maxX) * 0.5f;
    outCenter.Y = (minY + maxY) * 0.5f;
    outCenter.Z = (minZ + maxZ) * 0.5f;

    // Offset vertices by center to align geometry around local space (0,0,0)
    for (auto& v : outVertices) {
        v.x -= outCenter.X;
        v.y -= outCenter.Y;
        v.z -= outCenter.Z;
    }

    return true;
}

void Render3DObject(GameObject* obj, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd, const Matrix4x4& view, const Matrix4x4& proj, GpuPipeline& pipeline, const LightUBO& lightData, const std::vector<std::shared_ptr<GameObject>>& sceneObjects, bool isShadowPass = false) {
    if (!obj) return;
    if (obj->Is3D) {
        Vector3D worldPos = obj->GetWorldPosition();
        Vector3D worldRot = obj->GetWorldRotation();
        Vector3D worldScale = obj->GetWorldScale();

        float rotX = worldRot.X * 3.14159265f / 180.0f;
        float rotY = worldRot.Y * 3.14159265f / 180.0f;
        float rotZ = worldRot.Z * 3.14159265f / 180.0f;

        float scaleX = worldScale.X;
        float scaleY = worldScale.Y;
        float scaleZ = worldScale.Z;
        if (obj->ClassName == "LightSource") {
            scaleX *= 0.15f; scaleY *= 0.15f; scaleZ *= 0.15f;
        }

        float offRotX = obj->RotationOffset.X * 3.14159265f / 180.0f;
        float offRotY = obj->RotationOffset.Y * 3.14159265f / 180.0f;
        float offRotZ = obj->RotationOffset.Z * 3.14159265f / 180.0f;

        Matrix4x4 model = Matrix4x4::Translation(obj->PositionOffset.X, obj->PositionOffset.Y, obj->PositionOffset.Z)
            .Multiply(Matrix4x4::RotationZ(offRotZ))
            .Multiply(Matrix4x4::RotationX(offRotX))
            .Multiply(Matrix4x4::RotationY(offRotY))
            .Multiply(Matrix4x4::Scale(scaleX, scaleY, scaleZ))
            .Multiply(Matrix4x4::RotationZ(rotZ))
            .Multiply(Matrix4x4::RotationX(rotX))
            .Multiply(Matrix4x4::RotationY(rotY))
            .Multiply(Matrix4x4::Translation(worldPos.X, worldPos.Y, worldPos.Z));

        Matrix4x4 mvp = model.Multiply(view).Multiply(proj);
        
        struct VertexUniforms {
            Matrix4x4 MVP;
            Matrix4x4 Model;
            float ColorOverride[4];
        } vsUniform;
        vsUniform.MVP   = mvp;
        vsUniform.Model = model;
        vsUniform.ColorOverride[0] = 0.0f;
        vsUniform.ColorOverride[1] = 0.0f;
        vsUniform.ColorOverride[2] = 0.0f;
        vsUniform.ColorOverride[3] = 0.0f;

        // Bind vertex buffer & index buffer for this specific object
        SDL_GPUBufferBinding vertexBinding = {};
        vertexBinding.buffer = obj->CustomVertexBuffer ? obj->CustomVertexBuffer.get() : pipeline.VertexBuffer;
        vertexBinding.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
 
        SDL_GPUBufferBinding indexBinding = {};
        indexBinding.buffer = obj->CustomIndexBuffer ? obj->CustomIndexBuffer.get() : pipeline.IndexBuffer;
        indexBinding.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_PushGPUVertexUniformData(cmd, 0, &vsUniform, sizeof(vsUniform));
        if (!isShadowPass) SDL_PushGPUFragmentUniformData(cmd, 0, &lightData, sizeof(lightData));
        
        uint32_t indexCount = obj->CustomIndexBuffer ? obj->CustomIndexCount : 36;
        SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, 1, 0, 0, 0);
    }

    // Traverse children
    for (auto& child : obj->Children) {
        Render3DObject(child.get(), renderPass, cmd, view, proj, pipeline, lightData, sceneObjects, isShadowPass);
    }
}

int main() {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    // Create 3D Cube GameObject
    auto cubeObj = std::make_shared<GameObject>("3D Cube", Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f));

    // Create 3D LightSource GameObject
    auto lightObj = std::make_shared<LightSource>("LightSource");

    std::vector<std::shared_ptr<GameObject>> sceneObjects = { cubeObj, lightObj };
    std::vector<std::vector<std::shared_ptr<GameObject>>> undoStack;
    auto saveHistory = [&]() {
        undoStack.push_back(CloneScene(sceneObjects));
        if (undoStack.size() > 10) {
            undoStack.erase(undoStack.begin());
        }
    };
    auto addObject = [&](std::shared_ptr<GameObject> newObj) {
        saveHistory();
        sceneObjects.push_back(newObj);
    };

    // Tool mode enum for gizmo selection
    enum GizmoTool { ToolDrag, ToolMove, ToolRotate, ToolScale };
    GizmoTool currentTool = ToolDrag;

    GameObject* selectedObject = nullptr;
    bool showImportModal = false;
    SDL_GPUTexture* depthTexture = nullptr;
    Uint32 depthWidth = 0;
    Uint32 depthHeight = 0;

    // Interface Windows
    ExplorerWindow explorer;
    PropertiesWindow properties;
    ConsoleWindow console;

    // Script Engine
    ScriptManager::Initialize();

    // Camera Controller
    CameraController cameraController;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Art3DC++ Engine Viewport", 1920, 1080, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Initialize GPU Pipeline
    GpuPipeline pipeline;
    if (!pipeline.Initialize(window)) {
        SDL_Log("Failed to initialize GPU pipeline!");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Disable auto-load/save, we manage ini ourselves
    io.IniFilename = nullptr;

    // Load ini manually if it exists
    std::ifstream iniCheck("imgui.ini");
    if (iniCheck.good()) {
        iniCheck.close();
        ImGui::LoadIniSettingsFromDisk("imgui.ini");
    } 
    // If no ini: memory is blank → DockBuilder triggers → default layout

    // Re-enable saving to disk
    io.IniFilename = "imgui.ini";
    
    // Load Google Sans as the default UI font
    std::string uiFontPath = ResolveAssetPath("ttf/GoogleSans-Regular.ttf");
    ImFont* defaultFont = io.Fonts->AddFontFromFileTTF(uiFontPath.c_str(), 15.0f);
    if (!defaultFont) {
        io.Fonts->AddFontDefault();
    }

    // Merge Lucide icons into the UI font
    {
        ImFontConfig mergeCfg;
        mergeCfg.MergeMode = true;
        mergeCfg.GlyphOffset = ImVec2(0, 3);
        static const ImWchar lucideRanges[] = { 0xe000, 0xf000, 0 };
        io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/Lucide.ttf").c_str(), 15.0f, &mergeCfg, lucideRanges);
    }

    io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/GoogleSans-Bold.ttf").c_str(), 15.0f);

    // Merge Lucide icons into the bold font too
    {
        ImFontConfig mergeCfg;
        mergeCfg.MergeMode = true;
        mergeCfg.GlyphOffset = ImVec2(0, 3);
        static const ImWchar lucideRanges[] = { 0xe000, 0xf000, 0 };
        io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/Lucide.ttf").c_str(), 15.0f, &mergeCfg, lucideRanges);
    }

    // Load JetBrains Mono for code/script/console (global reference for panels that need mono)
    std::string monoFontPath = ResolveAssetPath("ttf/JetBrainsMono-Regular.ttf");
    g_MonoFont = io.Fonts->AddFontFromFileTTF(monoFontPath.c_str(), 17.0f);

    ImGui::StyleColorsDark();

    // Apply modern One Dark-inspired theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 4.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(6, 5);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.67f, 0.70f, 0.75f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.36f, 0.39f, 0.44f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.17f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.17f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_Border]                = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.17f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.29f, 0.32f, 0.39f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.35f, 0.38f, 0.47f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.46f, 0.76f, 0.97f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.29f, 0.32f, 0.39f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.38f, 0.47f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.29f, 0.32f, 0.39f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.35f, 0.38f, 0.47f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.46f, 0.76f, 0.97f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.46f, 0.76f, 0.97f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.17f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.17f, 0.19f, 0.22f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.38f, 0.69f, 0.94f, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.46f, 0.76f, 0.97f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.60f, 0.76f, 0.40f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.69f, 0.82f, 0.52f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_NavCursor]             = ImVec4(0.38f, 0.69f, 0.94f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    ImGui_ImplSDL3_InitForSDLGPU(window);

    ImGui_ImplSDLGPU3_InitInfo initInfo = {};
    initInfo.Device = pipeline.Device;
    initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(pipeline.Device, window);
    initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&initInfo);

    console.Initialize();

    // Set up explorer insert-as-child callback
    explorer.SetInsertCallback([&](GameObject* parent, const std::string& type) {
        saveHistory();
        std::shared_ptr<GameObject> newObj;
        if (type == "3D") {
            newObj = std::make_shared<GameObject>("3D Object", Vector3D(0, 0, 0), Vector3D(0, 0, 0), Vector3D(1, 1, 1));
        } else if (type == "Light") {
            newObj = std::make_shared<LightSource>("LightSource");
        } else if (type == "Script") {
            newObj = std::make_shared<LogicScript>("LogicScript");
        }
        if (newObj) {
            newObj->GpuDevice = pipeline.Device;
            parent->AddChild(newObj);
            selectedObject = newObj.get();
        }
    });

    // Frame rate limiter variables
    const int TARGET_FPS = 120;
    const Uint64 FRAME_TARGET_TIME = 1000 / TARGET_FPS;
    Uint64 frameStartTicks;
    Uint64 frameTime;

    // Gizmo drag state
    int  dragAxis    = -1; // 0=X, 1=Y, 2=Z, -1=none
    float lastMX     = 0.f, lastMY = 0.f;

    bool running = true;
    SDL_Event event;
    while (running) {
        frameStartTicks = SDL_GetTicks();
 
        while (SDL_PollEvent(&event)) {
            bool intercepted = false;
            if (g_autocompleteOpen) {
                if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                    SDL_Keycode key = event.key.key;
                    if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_RETURN || key == SDLK_TAB || key == SDLK_ESCAPE) {
                        intercepted = true;
                        if (event.type == SDL_EVENT_KEY_DOWN) {
                            if (key == SDLK_UP) {
                                g_autocompleteActiveIndex--;
                            } else if (key == SDLK_DOWN) {
                                g_autocompleteActiveIndex++;
                            } else if (key == SDLK_RETURN || key == SDLK_TAB) {
                                g_autocompleteSelectTriggered = true;
                            } else if (key == SDLK_ESCAPE) {
                                g_autocompleteCloseTriggered = true;
                            }
                        }
                    }
                }
            }

            if (!intercepted) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (!ImGui::GetIO().WantTextInput) {
                    cameraController.ProcessEvent(event, window);
                } else {
                    cameraController.ReleaseMouse(window);
                }
            } else {
                if (ImGui::GetIO().WantTextInput) {
                    cameraController.ReleaseMouse(window);
                }
            }
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }

            // ─── Left-click: check gizmo handles first, then scene objects ───
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    int winW, winH;
                    SDL_GetWindowSize(window, &winW, &winH);
                    float mouseX = event.button.x;
                    float mouseY = event.button.y;

                    // Build pick ray
                    float ndcX = (2.0f * mouseX) / (float)winW - 1.0f;
                    float ndcY = 1.0f - (2.0f * mouseY) / (float)winH;
                    float cosPitch = cosf(cameraController.Pitch);
                    float sinPitch = sinf(cameraController.Pitch);
                    float sinYaw   = sinf(cameraController.Yaw);
                    float cosYaw   = cosf(cameraController.Yaw);
                    float fwdX = sinYaw * cosPitch, fwdY = -sinPitch, fwdZ = cosYaw * cosPitch;
                    float rgtX = cosYaw,            rgtZ = -sinYaw;
                    float upX  = fwdY * rgtZ,       upY  = fwdZ * rgtX - fwdX * rgtZ, upZ = -fwdY * rgtX;
                    float aspect = (float)winW / (float)winH;
                    float tanHFov = tanf(45.0f * 3.14159265f / 180.0f * 0.5f);
                    float sx = ndcX * tanHFov * aspect, sy = ndcY * tanHFov;
                    float dirX = fwdX + rgtX*sx + upX*sy;
                    float dirY = fwdY + upY*sy;
                    float dirZ = fwdZ + rgtZ*sx + upZ*sy;
                    float dlen = sqrtf(dirX*dirX + dirY*dirY + dirZ*dirZ);
                    dirX /= dlen; dirY /= dlen; dirZ /= dlen;
                    float origX = cameraController.X, origY = cameraController.Y, origZ = cameraController.Z;

                    // Helper: ray-AABB returning t or -1
                    auto rayAABB = [&](float cx, float cy, float cz, float hx, float hy, float hz) -> float {
                        float t1x = (cx - hx - origX) / dirX, t2x = (cx + hx - origX) / dirX;
                        if (t1x > t2x) std::swap(t1x, t2x);
                        float t1y = (cy - hy - origY) / dirY, t2y = (cy + hy - origY) / dirY;
                        if (t1y > t2y) std::swap(t1y, t2y);
                        float t1z = (cz - hz - origZ) / dirZ, t2z = (cz + hz - origZ) / dirZ;
                        if (t1z > t2z) std::swap(t1z, t2z);
                        float tMin = std::max({t1x, t1y, t1z});
                        float tMax = std::min({t2x, t2y, t2z});
                        if (tMax < 0 || tMin > tMax) return -1.f;
                        return (tMin >= 0.f) ? tMin : tMax;
                    };

                    // Check gizmo handles if a gizmo tool is active
                    dragAxis = -1;
                    if (selectedObject && currentTool != ToolDrag && currentTool != ToolRotate && currentTool != ToolScale) {
                        // TODO: for ToolRotate and ToolScale, implement their own handle hit-testing
                        Vector3D worldPos = selectedObject->GetWorldPosition();
                        float px = worldPos.X;
                        float py = worldPos.Y;
                        float pz = worldPos.Z;

                        Vector3D worldScale = selectedObject->GetWorldScale();
                        float hx = worldScale.X * 0.5f;
                        float hy = worldScale.Y * 0.5f;
                        float hz = worldScale.Z * 0.5f;

                        // Each axis: shaft AABB + tip AABB (slightly enlarged for easier clicking)
                        struct AxisRegion { float cx,cy,cz, hx,hy,hz; int axis; };
                        AxisRegion regions[] = {
                            // X shaft, X tip
                            { px+hx+0.35f, py, pz,  0.26f, 0.04f, 0.04f,  0 },
                            { px+hx+0.62f, py, pz,  0.07f, 0.07f, 0.07f,  0 },
                            // Y shaft, Y tip
                            { px, py+hy+0.35f, pz,  0.04f, 0.26f, 0.04f,  1 },
                            { px, py+hy+0.62f, pz,  0.07f, 0.07f, 0.07f,  1 },
                            // Z shaft, Z tip
                            { px, py, pz+hz+0.35f,  0.04f, 0.04f, 0.26f,  2 },
                            { px, py, pz+hz+0.62f,  0.07f, 0.07f, 0.07f,  2 },
                        };
                        float bestT = 1e9f;
                        for (auto& r : regions) {
                            float t = rayAABB(r.cx, r.cy, r.cz, r.hx, r.hy, r.hz);
                            if (t >= 0.f && t < bestT) { bestT = t; dragAxis = r.axis; }
                        }
                        if (dragAxis >= 0) {
                            saveHistory();
                            lastMX = mouseX; lastMY = mouseY;
                            // Don't do scene selection when we grabbed a handle
                        }
                    }

                    // If no handle grabbed, do scene object selection
                    if (dragAxis < 0) {
                        selectedObject = nullptr;
                        float closestT = 1e9f;
                        for (auto& obj : sceneObjects) {
                            if (!obj->Is3D) continue;
                            float hx = obj->Scale3D.X * 0.5f;
                            float hy = obj->Scale3D.Y * 0.5f;
                            float hz = obj->Scale3D.Z * 0.5f;
                            if (obj->ClassName == "LightSource") { hx *= 0.15f; hy *= 0.15f; hz *= 0.15f; }
                            Vector3D worldPos = GetFinalWorldCenter(obj.get());
                            float t = rayAABB(worldPos.X, worldPos.Y, worldPos.Z, hx, hy, hz);
                            if (t >= 0.f && t < closestT) { closestT = t; selectedObject = obj.get(); }
                        }
                    }
                }
            }

            // ─── Left-release: stop dragging ───
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                dragAxis = -1;
            }

            // ─── Mouse motion: drag selected object along chosen axis ───
            if (event.type == SDL_EVENT_MOUSE_MOTION && dragAxis >= 0 && selectedObject) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    int winW, winH;
                    SDL_GetWindowSize(window, &winW, &winH);
                    float curMX = event.motion.x;
                    float curMY = event.motion.y;
                    float dmx = curMX - lastMX;
                    float dmy = curMY - lastMY;
                    lastMX = curMX; lastMY = curMY;

                    // Project the world-space drag axis onto screen space
                    // Then use (mouse delta · screen axis) * scale as movement amount
                    float cosPitch = cosf(cameraController.Pitch);
                    float sinPitch = sinf(cameraController.Pitch);
                    float sinYaw   = sinf(cameraController.Yaw);
                    float cosYaw   = cosf(cameraController.Yaw);
                    float fwdX = sinYaw * cosPitch, fwdY = -sinPitch, fwdZ = cosYaw * cosPitch;
                    float rgtX = cosYaw,            rgtZ = -sinYaw;
                    float upX  = fwdY * rgtZ,       upY  = fwdZ * rgtX - fwdX * rgtZ, upZ = -fwdY * rgtX;

                    // World-space drag axis direction
                    float axW[3] = { (dragAxis==0)?1.f:0.f, (dragAxis==1)?1.f:0.f, (dragAxis==2)?1.f:0.f };

                    // Project axis direction onto camera right/up to get 2D screen direction
                    float screenAxX = axW[0]*rgtX + axW[2]*rgtZ;          // dot(axis, right)
                    float screenAxY = -(axW[0]*upX + axW[1]*upY + axW[2]*upZ); // dot(axis, up), flip Y (screen Y down)

                    // Normalise screen axis
                    float slen = sqrtf(screenAxX*screenAxX + screenAxY*screenAxY);
                    if (slen > 0.001f) { screenAxX /= slen; screenAxY /= slen; }

                    // Dot mouse delta with screen axis to get signed movement
                    float move = dmx * screenAxX + dmy * screenAxY;

                    // Scale by world depth (distance to object) so pixel movement feels consistent
                    float dx = selectedObject->Position3D.X - cameraController.X;
                    float dy = selectedObject->Position3D.Y - cameraController.Y;
                    float dz = selectedObject->Position3D.Z - cameraController.Z;
                    float depth = sqrtf(dx*dx + dy*dy + dz*dz);
                    float aspect = (float)winW / (float)winH;
                    float tanHFov = tanf(45.0f * 3.14159265f / 180.0f * 0.5f);
                    float worldScale = depth * tanHFov * 2.0f / (float)winH;

                    move *= worldScale;

                    if (dragAxis == 0) selectedObject->Position3D.X += move;
                    if (dragAxis == 1) selectedObject->Position3D.Y += move;
                    if (dragAxis == 2) selectedObject->Position3D.Z += move;
                }
            }
        }

        // Update Camera walking input if we are not typing in a text field and keyboard is not captured by ImGui
        if (!ImGui::GetIO().WantTextInput && !ImGui::GetIO().WantCaptureKeyboard) {
            cameraController.Update();
        }
 
        // Start the Dear ImGui frame
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Handle shortcut keys
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I) && !io.WantTextInput) {
            showImportModal = true;
        }

        // Tool switching shortcuts (Q=Drag, W=Move, E=Rotate, R=Scale)
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_1)) currentTool = ToolDrag;
            if (ImGui::IsKeyPressed(ImGuiKey_2)) currentTool = ToolMove;
            if (ImGui::IsKeyPressed(ImGuiKey_3)) currentTool = ToolRotate;
            if (ImGui::IsKeyPressed(ImGuiKey_4)) currentTool = ToolScale;
        }

        // Handle Ctrl+Z (Undo)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.WantTextInput) {
            if (!undoStack.empty()) {
                std::string selectedName = selectedObject ? selectedObject->Name : "";
                sceneObjects = undoStack.back();
                undoStack.pop_back();
                if (!selectedName.empty()) {
                    selectedObject = FindSelectedInScene(sceneObjects, selectedName);
                } else {
                    selectedObject = nullptr;
                }
            }
        }

        // Handle Delete key to delete selected object
        if (selectedObject && ImGui::IsKeyPressed(ImGuiKey_Delete) && !io.WantTextInput) {
            saveHistory();
            DeleteObject(selectedObject, sceneObjects);
            selectedObject = nullptr;
        }

        // Render main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    running = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undoStack.empty())) {
                    std::string selectedName = selectedObject ? selectedObject->Name : "";
                    if (!undoStack.empty()) {
                        sceneObjects = undoStack.back();
                        undoStack.pop_back();
                        if (!selectedName.empty()) {
                            selectedObject = FindSelectedInScene(sceneObjects, selectedName);
                        } else {
                            selectedObject = nullptr;
                        }
                    }
                }
                if (ImGui::MenuItem("Delete Selected", "Del", false, selectedObject != nullptr)) {
                    saveHistory();
                    DeleteObject(selectedObject, sceneObjects);
                    selectedObject = nullptr;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Import Object...", "Ctrl+I")) {
                    showImportModal = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    g_resetDockspaceLayout = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        // Render Import Modal
        if (showImportModal) {
            ImGui::OpenPopup("Import Object");
        }
        if (ImGui::BeginPopupModal("Import Object", &showImportModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Spacing();

            // 3D Objects
            if (ImGui::TreeNodeEx(ICON_CUBOID " 3D Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::MenuItem(ICON_CUBOID "  3D Cube (GameObject)")) {
                    auto newObj = std::make_shared<GameObject>("3D Cube", Vector3D(0, 0, 0), Vector3D(0, 0, 0), Vector3D(1, 1, 1));
                    addObject(newObj);
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::MenuItem(ICON_SUN "  LightSource")) {
                    auto newObj = std::make_shared<LightSource>("LightSource");
                    addObject(newObj);
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::MenuItem(ICON_BOX "  Part")) {
                    auto newObj = std::make_shared<Part>("Part");
                    addObject(newObj);
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::TreePop();
                ImGui::Spacing();
            }

            // 2D UI Elements
            if (ImGui::TreeNodeEx(ICON_LAYOUT " 2D UI Elements", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::MenuItem(ICON_LAYOUT "  2D Frame")) {
                    auto newObj = std::make_shared<Frame>("Frame", Position2D(0, 0, 100, 100), Size2D(0, 0, 200, 150));
                    addObject(newObj);
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::MenuItem(ICON_LAYOUT "  2D Panel (GameObject)")) {
                    auto newObj = std::make_shared<GameObject>("Panel", Position2D(0, 0, 150, 150), Size2D(0, 0, 300, 200));
                    addObject(newObj);
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::TreePop();
                ImGui::Spacing();
            }

            // Scripts & Logic
            if (ImGui::TreeNodeEx(ICON_FILE_CODE " Scripts & Logic", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::MenuItem(ICON_FILE_CODE "  Logic Script")) {
                    auto newObj = std::make_shared<LogicScript>("LogicScript");
                    addObject(newObj);
                    selectedObject = newObj.get();
                    showImportModal = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::TreePop();
                ImGui::Spacing();
            }

            // Import external models
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled(ICON_IMPORT " Import External Models");
            static char objPathBuf[256] = "suzanne.obj";
            ImGui::PushItemWidth(250);
            ImGui::InputText("##objpath", objPathBuf, sizeof(objPathBuf));
            ImGui::PopItemWidth();
            if (ImGui::MenuItem(ICON_IMPORT "  Import wavefront .obj")) {
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;
                Vector3D center(0, 0, 0);
                if (LoadObj(objPathBuf, vertices, indices, center)) {
                    SDL_GPUBuffer* rawVB = pipeline.CreateAndUploadBuffer(vertices.data(), (Uint32)(vertices.size() * sizeof(Vertex)), SDL_GPU_BUFFERUSAGE_VERTEX);
                    SDL_GPUBuffer* rawIB = pipeline.CreateAndUploadBuffer(indices.data(), (Uint32)(indices.size() * sizeof(uint32_t)), SDL_GPU_BUFFERUSAGE_INDEX);
                    if (rawVB && rawIB) {
                        auto newObj = std::make_shared<GameObject>("Imported OBJ", center, Vector3D(0, 0, 0), Vector3D(1, 1, 1));
                        newObj->CustomVertexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawVB, GpuBufferDeleter{pipeline.Device});
                        newObj->CustomIndexBuffer = std::shared_ptr<SDL_GPUBuffer>(rawIB, GpuBufferDeleter{pipeline.Device});
                        newObj->CustomIndexCount = (uint32_t)indices.size();
                        newObj->GpuDevice = pipeline.Device;

                        addObject(newObj);
                        showImportModal = false;
                        ImGui::CloseCurrentPopup();
                    } else {
                        SDL_Log("Failed to create GPU buffers for OBJ model.");
                    }
                } else {
                    SDL_Log("Failed to parse OBJ file or file not found.");
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::MenuItem(ICON_X "  Cancel")) {
                showImportModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 1. Create dockspace FIRST
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        static bool dockspace_initialized = false;

        if (!dockspace_initialized || g_resetDockspaceLayout) {
            dockspace_initialized = true;

            // Only rebuild layout if no ini was loaded (first ever launch)
            // or if user explicitly requested a reset
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
            bool hasLayout = (node != nullptr && node->ChildNodes[0] != nullptr);

            if (!hasLayout || g_resetDockspaceLayout) {
                g_resetDockspaceLayout = false;

                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
                ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_id_bottom, dock_id_left, dock_id_right;

                ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down,  0.25f, &dock_id_bottom, &dock_main_id);
                ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left,  0.20f, &dock_id_left,   &dock_main_id);
                ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, &dock_id_right,  &dock_main_id);

                ImGui::DockBuilderDockWindow("Console",       dock_id_bottom);
                ImGui::DockBuilderDockWindow("Properties",    dock_id_left);
                ImGui::DockBuilderDockWindow("Explorer",      dock_id_right);
                ImGui::DockBuilderDockWindow("Script Editor", dock_main_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }

        // static bool dockspace_initialized = false;
        // bool needs_init = g_resetDockspaceLayout;
        // if (!needs_init && !dockspace_initialized) {
        //     ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        //     if (node == nullptr || node->ChildNodes[0] == nullptr) {
        //         needs_init = true;
        //     }
        // }

        // if (needs_init) {
        //     dockspace_initialized = true;
        //     g_resetDockspaceLayout = false;

        //     ImGui::DockBuilderRemoveNode(dockspace_id);
        //     ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);

        //     ImGuiViewport* viewport = ImGui::GetMainViewport();
        //     ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        //     ImGuiID dock_main_id = dockspace_id;
        //     ImGuiID dock_id_bottom, dock_id_left, dock_id_right;

        //     // Split bottom for Console — remaining top portion stays in dock_main_id
        //     ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, &dock_id_bottom, &dock_main_id);

        //     // Split left for Properties — remaining center stays in dock_main_id  
        //     ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, &dock_id_left, &dock_main_id);

        //     // Split right for Explorer — remaining center stays in dock_main_id
        //     ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, &dock_id_right, &dock_main_id);

        //     // dock_main_id is now the center viewport node
        //     ImGui::DockBuilderDockWindow("Console",       dock_id_bottom);
        //     ImGui::DockBuilderDockWindow("Properties",    dock_id_left);
        //     ImGui::DockBuilderDockWindow("Explorer",      dock_id_right);
        //     ImGui::DockBuilderDockWindow("Script Editor", dock_main_id);

        //     ImGui::DockBuilderFinish(dockspace_id);
        // } else {
        //     dockspace_initialized = true;
        // }

        // 3. Draw windows AFTER dockspace is set up
        explorer.Draw(sceneObjects, selectedObject);

        bool saveHistoryFromProps = false;
        properties.Draw(selectedObject, saveHistoryFromProps);
        if (saveHistoryFromProps) {
            saveHistory();
        }

        console.Draw();

        // Sync open scripts list and clean up any deleted scripts or re-map cloned scripts
        SyncOpenScripts(sceneObjects);

        if (selectedObject && selectedObject->ClassName == "LogicScript") {
            LogicScript* script = static_cast<LogicScript*>(selectedObject);
            // Add to open scripts if not already open
            bool found = false;
            for (const auto& info : g_openScripts) {
                if (info.ptr == script) {
                    found = true;
                    break;
                }
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
            // "to get out simply click empty space on explorer or whatever"
            g_showScriptEditor = false;
            g_lastSelectedObject = selectedObject;
        }

        // Render Script Editor Window with TabBar if g_showScriptEditor is true and we have open scripts
        if (g_showScriptEditor && !g_openScripts.empty()) {
            ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Script Editor", &g_showScriptEditor, ImGuiWindowFlags_MenuBar)) {
                static float scriptFontSize = 30.0f;

                // Ctrl + Scroll wheel to adjust font size when script editor window is hovered
                ImGuiIO& io = ImGui::GetIO();
                if (io.KeyCtrl && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
                    if (io.MouseWheel != 0.0f) {
                        scriptFontSize += io.MouseWheel * 1.0f;
                        if (scriptFontSize < 8.0f) scriptFontSize = 8.0f;
                        if (scriptFontSize > 72.0f) scriptFontSize = 72.0f;
                        io.MouseWheel = 0.0f; // Consume mouse wheel
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
                            // Auto-register exports on first open (no console spam)
                            ScriptManager::RegisterScript(script->Name, script->SourceCode);
                        }

                        ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
                        if (g_scriptToSelect == script) {
                            tabFlags |= ImGuiTabItemFlags_SetSelected;
                        }

                        std::string tabLabel = script->Name;
                        if (tabLabel.empty()) {
                            tabLabel = "Unnamed Script";
                        }
                        tabLabel += "##" + std::to_string((uintptr_t)script);

                        bool open = true;
                        if (ImGui::BeginTabItem(tabLabel.c_str(), &open, tabFlags)) {
                            // If user selected this tab, sync the selectedObject
                            if (selectedObject != script) {
                                selectedObject = script;
                                g_lastSelectedObject = script;
                            }

                            // Render the text editor
                            if (g_MonoFont) ImGui::PushFont(g_MonoFont, scriptFontSize);
                            editor.Render("##codeEditor");
                            if (g_MonoFont) ImGui::PopFont();

                            // Force ImGui context for text input
                            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                                ImGui::GetCurrentContext()->PlatformImeData.WantTextInput = true;
                            }

                            // Autocomplete suggestion logic
                            std::string currentWord = GetCurrentWordPrefix(editor);
                            bool showAutocomplete = false;

                            // Reset autocomplete dismissal when text changes (user is typing)
                            if (editor.IsTextChanged()) {
                                g_autocompleteDismissed = false;
                            }

                            if (!currentWord.empty() && std::isalpha((unsigned char)currentWord[0]) && !g_autocompleteDismissed) {
                                g_autocompleteMatches.clear();
                                for (const auto& keyword : combinedKeywordsList) {
                                    if (keyword.size() >= currentWord.size() && keyword.compare(0, currentWord.size(), currentWord) == 0) {
                                        g_autocompleteMatches.push_back(keyword);
                                    }
                                }
                                if (!g_autocompleteMatches.empty()) {
                                    showAutocomplete = true;
                                }
                            }

                            if (showAutocomplete) {
                                g_autocompleteOpen = true;

                                if (g_autocompleteActiveIndex < 0) {
                                    g_autocompleteActiveIndex = (int)g_autocompleteMatches.size() - 1;
                                }
                                if (g_autocompleteActiveIndex >= (int)g_autocompleteMatches.size()) {
                                    g_autocompleteActiveIndex = 0;
                                }

                                if (g_autocompleteSelectTriggered) {
                                    InsertSuggestion(editor, g_autocompleteMatches[g_autocompleteActiveIndex]);
                                    g_autocompleteSelectTriggered = false;
                                    g_autocompleteOpen = false;
                                    showAutocomplete = false;
                                } else if (g_autocompleteCloseTriggered) {
                                    g_autocompleteCloseTriggered = false;
                                    g_autocompleteOpen = false;
                                    showAutocomplete = false;
                                    g_autocompleteDismissed = true;
                                }

                                if (showAutocomplete) {
                                    ImVec2 cursorScreenPos = editor.GetCursorScreenPos();
                                    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
                                    ImGui::SetNextWindowPos(ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineHeight));
                                    ImGui::SetNextWindowSizeConstraints(ImVec2(150, 50), ImVec2(300, 200));

                                    ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar | 
                                                                 ImGuiWindowFlags_NoResize | 
                                                                 ImGuiWindowFlags_NoMove | 
                                                                 ImGuiWindowFlags_HorizontalScrollbar | 
                                                                 ImGuiWindowFlags_AlwaysAutoResize | 
                                                                 ImGuiWindowFlags_NoSavedSettings | 
                                                                 ImGuiWindowFlags_Tooltip;

                                    if (ImGui::Begin("##autocompletePopup", nullptr, popupFlags)) {
                                        for (int j = 0; j < (int)g_autocompleteMatches.size(); j++) {
                                            bool isSelected = (j == g_autocompleteActiveIndex);
                                            if (isSelected) {
                                                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                                            }

                                            if (ImGui::Selectable(g_autocompleteMatches[j].c_str(), isSelected)) {
                                                InsertSuggestion(editor, g_autocompleteMatches[j]);
                                                g_autocompleteOpen = false;
                                                showAutocomplete = false;
                                            }

                                            if (isSelected) {
                                                ImGui::PopStyleColor();
                                                if (ImGui::IsWindowAppearing()) {
                                                    ImGui::SetScrollHereY();
                                                }
                                            }
                                        }
                                    }
                                    ImGui::End();
                                }
                            } else {
                                g_autocompleteOpen = false;
                            }

                            // Save code changes back to script object if modified
                            if (editor.IsTextChanged()) {
                                script->SourceCode = editor.GetText();
                            }

                            // Trigger script execution on Ctrl+Enter
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

                        if (g_scriptToSelect == script) {
                            g_scriptToSelect = nullptr;
                        }

                        if (!open) {
                            scriptToClose = script;
                        }
                    }

                    if (scriptToClose != nullptr) {
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

        if (!g_showScriptEditor) {
            // Deselect the script if currently selected but editor is closed
            if (selectedObject && selectedObject->ClassName == "LogicScript") {
                selectedObject = nullptr;
                g_lastSelectedObject = nullptr;
            }
        }

        // Render fixed top toolbar
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() + 30;
            const float windowWidth = viewport->WorkSize.x;

            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(windowWidth, toolbarHeight), ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGuiWindowFlags toolbarFlags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;

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

                auto ToolButton = [&](const char* icon, GizmoTool tool, const char* tooltip, const char* shortcut) {
                    bool active = (currentTool == tool);
                    if (active) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                    }
                    if (ImGui::Button(icon, ImVec2(58, 58))) {
                        currentTool = tool;
                    }
                    if (active) ImGui::PopStyleColor(2);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        std::string tip = tooltip;
                        tip += " (";
                        tip += shortcut;
                        tip += ")";
                        ImGui::SetTooltip("%s", tip.c_str());
                    }
                };

                ToolButton(ICON_DRAG, ToolDrag, "Drag", "1");
                ImGui::SameLine(0, 0);
                ToolButton(ICON_MOVE, ToolMove, "Move", "2");
                ImGui::SameLine(0, 0);
                ToolButton(ICON_ROTATE, ToolRotate, "Rotate", "3");
                ImGui::SameLine(0, 0);
                ToolButton(ICON_SCALE, ToolScale, "Scale", "4");
            }
            ImGui::End();

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor();
        }

        // Render 2D User Interface GameObjects
        for (auto& obj : sceneObjects) {
            Render2DObject(obj.get());
        }

        // Render ImGui Draw Data
        ImGui::Render();
  
        // 1. Acquire command buffer
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(pipeline.Device);
        if (cmd) {
            // 2. Prepare ImGui draw data (must be done before the render pass starts)
            ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
 
            // 3. Acquire Swapchain texture
            SDL_GPUTexture* swapchainTexture = nullptr;
            Uint32 w = 0, h = 0;
            if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, &w, &h) && swapchainTexture != nullptr) {
                // Manage Depth-Stencil Texture
                if (depthTexture == nullptr || depthWidth != w || depthHeight != h) {
                    if (depthTexture != nullptr) {
                        SDL_ReleaseGPUTexture(pipeline.Device, depthTexture);
                    }
                    SDL_GPUTextureCreateInfo depthInfo = {};
                    depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
                    depthInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
                    depthInfo.width = w;
                    depthInfo.height = h;
                    depthInfo.layer_count_or_depth = 1;
                    depthInfo.num_levels = 1;
                    depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                    depthTexture = SDL_CreateGPUTexture(pipeline.Device, &depthInfo);
                    depthWidth = w;
                    depthHeight = h;
                }

                // 4. Begin render pass
                SDL_GPUColorTargetInfo colorTargetInfo = {};
                colorTargetInfo.texture = swapchainTexture;
                colorTargetInfo.clear_color = { 20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f }; // Dark gray background
                colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
                colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
 
                SDL_GPUDepthStencilTargetInfo depthTargetInfo = {};
                depthTargetInfo.texture = depthTexture;
                depthTargetInfo.clear_depth = 1.0f;
                depthTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
                depthTargetInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
                depthTargetInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
                depthTargetInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
                depthTargetInfo.cycle = true;

                SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, &depthTargetInfo);
                if (renderPass) {
                    // Set viewport
                    SDL_GPUViewport viewport = {};
                    viewport.min_depth = 0.0f;
                    viewport.max_depth = 1.0f;
                    viewport.x = 0.0f;
                    viewport.y = 0.0f;
                    viewport.w = (float)w;
                    viewport.h = (float)h;
                    SDL_SetGPUViewport(renderPass, &viewport);
 
                    // Bind graphics pipeline
                    SDL_BindGPUGraphicsPipeline(renderPass, pipeline.Pipeline);
 
                    // Bind vertex buffer & index buffer
                    SDL_GPUBufferBinding vertexBinding = {};
                    vertexBinding.buffer = pipeline.VertexBuffer;
                    vertexBinding.offset = 0;
                    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
 
                    SDL_GPUBufferBinding indexBinding = {};
                    indexBinding.buffer = pipeline.IndexBuffer;
                    indexBinding.offset = 0;
                    SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
 
                    float aspect = (h > 0) ? ((float)w / (float)h) : 1.0f;
                    Matrix4x4 proj = Matrix4x4::Perspective(45.0f, aspect, 0.1f, 100.0f);
                    Matrix4x4 view = cameraController.GetViewMatrix();

                    // Collect light data from scene (up to 8 lights)
                    LightUBO lightData = {};
                    for (auto& obj : sceneObjects) {
                        if (obj->ClassName == "LightSource" && lightData.lightCount < 8) {
                            LightSource* light = static_cast<LightSource*>(obj.get());
                            Vector3D worldPos = light->GetWorldPosition();
                            auto& gpuLight = lightData.lights[lightData.lightCount];
                            gpuLight.pos[0] = worldPos.X;
                            gpuLight.pos[1] = worldPos.Y;
                            gpuLight.pos[2] = worldPos.Z;
                            gpuLight.pos[3] = light->Intensity;
                            gpuLight.dir[0] = light->Direction.X;
                            gpuLight.dir[1] = light->Direction.Y;
                            gpuLight.dir[2] = light->Direction.Z;
                            gpuLight.dir[3] = light->Range;
                            gpuLight.color[0] = light->Color.X;
                            gpuLight.color[1] = light->Color.Y;
                            gpuLight.color[2] = light->Color.Z;
                            gpuLight.color[3] = (float)light->Type;
                            float innerRad = cosf(light->SpotAngleInner * 3.14159265f / 180.0f);
                            float outerRad = cosf(light->SpotAngleOuter * 3.14159265f / 180.0f);
                            gpuLight.spotAngles[0] = innerRad;
                            gpuLight.spotAngles[1] = outerRad;
                            lightData.lightCount++;
                        }
                    }
                    lightData.ambientIntensity = 0.15f;

                    // Render ground grid
                    {
                        SDL_GPUBufferBinding gridVB = {};
                        gridVB.buffer = pipeline.GridBuffer;
                        gridVB.offset = 0;
                        SDL_BindGPUVertexBuffers(renderPass, 0, &gridVB, 1);

                        SDL_GPUBufferBinding gridIB = {};
                        gridIB.buffer = pipeline.GridIndexBuffer;
                        gridIB.offset = 0;
                        SDL_BindGPUIndexBuffer(renderPass, &gridIB, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                        Matrix4x4 gridModel = Matrix4x4::Identity();
                        Matrix4x4 gridMVP = gridModel.Multiply(view).Multiply(proj);

                        struct GridUniforms {
                            Matrix4x4 MVP;
                            Matrix4x4 Model;
                            float ColorOverride[4];
                        } gridUniform;
                        gridUniform.MVP = gridMVP;
                        gridUniform.Model = gridModel;
                        gridUniform.ColorOverride[0] = 0.0f;
                        gridUniform.ColorOverride[1] = 0.0f;
                        gridUniform.ColorOverride[2] = 0.0f;
                        gridUniform.ColorOverride[3] = 0.0f;

                        SDL_PushGPUVertexUniformData(cmd, 0, &gridUniform, sizeof(gridUniform));
                        SDL_PushGPUFragmentUniformData(cmd, 0, &lightData, sizeof(lightData));
                        SDL_DrawGPUIndexedPrimitives(renderPass, pipeline.GridIndexCount, 1, 0, 0, 0);
                    }

                    // Re-bind default cube buffers for objects
                    {
                        SDL_GPUBufferBinding vb = {};
                        vb.buffer = pipeline.VertexBuffer;
                        vb.offset = 0;
                        SDL_BindGPUVertexBuffers(renderPass, 0, &vb, 1);

                        SDL_GPUBufferBinding ib = {};
                        ib.buffer = pipeline.IndexBuffer;
                        ib.offset = 0;
                        SDL_BindGPUIndexBuffer(renderPass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                    }

                    // Render all 3D GameObjects
                    for (auto& obj : sceneObjects) {
                        Render3DObject(obj.get(), renderPass, cmd, view, proj, pipeline, lightData, sceneObjects);
                    }

                    // Render 3D Gizmo handles based on active tool mode
                    if (selectedObject && currentTool != ToolDrag) {
                        // Re-bind the default pipeline buffers for the gizmo handles
                        SDL_GPUBufferBinding vertexBinding = {};
                        vertexBinding.buffer = pipeline.VertexBuffer;
                        vertexBinding.offset = 0;
                        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

                        SDL_GPUBufferBinding indexBinding = {};
                        indexBinding.buffer = pipeline.IndexBuffer;
                        indexBinding.offset = 0;
                        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                        struct Handle {
                            Vector3D scale;
                            Vector3D offset;
                            float r, g, b, a;
                        };

                        Vector3D worldPos = selectedObject->GetWorldPosition();
                        Vector3D worldScale = selectedObject->GetWorldScale();
                        float hx = worldScale.X * 0.5f;
                        float hy = worldScale.Y * 0.5f;
                        float hz = worldScale.Z * 0.5f;

                        // Render each handle set based on tool mode
                        auto renderHandles = [&](Handle* list, int count) {
                            for (int i = 0; i < count; i++) {
                                const auto& h = list[i];
                                float ox = h.offset.X > 0.0f ? hx : 0.0f;
                                float oy = h.offset.Y > 0.0f ? hy : 0.0f;
                                float oz = h.offset.Z > 0.0f ? hz : 0.0f;

                                Matrix4x4 model = Matrix4x4::Scale(h.scale.X, h.scale.Y, h.scale.Z)
                                    .Multiply(Matrix4x4::Translation(
                                        worldPos.X + ox + h.offset.X,
                                        worldPos.Y + oy + h.offset.Y,
                                        worldPos.Z + oz + h.offset.Z
                                    ));
                                Matrix4x4 mvp = model.Multiply(view).Multiply(proj);

                                struct VertexUniforms {
                                    Matrix4x4 MVP;
                                    Matrix4x4 Model;
                                    float ColorOverride[4];
                                } vsUniform;
                                vsUniform.MVP   = mvp;
                                vsUniform.Model = model;
                                vsUniform.ColorOverride[0] = h.r;
                                vsUniform.ColorOverride[1] = h.g;
                                vsUniform.ColorOverride[2] = h.b;
                                vsUniform.ColorOverride[3] = h.a;

                                SDL_PushGPUVertexUniformData(cmd, 0, &vsUniform, sizeof(vsUniform));
                                SDL_PushGPUFragmentUniformData(cmd, 0, &lightData, sizeof(lightData));
                                SDL_DrawGPUIndexedPrimitives(renderPass, 36, 1, 0, 0, 0);
                            }
                        };

                        if (currentTool == ToolMove) {
                            Handle ha[] = {
                                { Vector3D(0.50f, 0.02f, 0.02f), Vector3D(0.35f, 0.0f, 0.0f), 1.0f, 0.2f, 0.2f, 1.0f },
                                { Vector3D(0.06f, 0.06f, 0.06f), Vector3D(0.62f, 0.0f, 0.0f), 1.0f, 0.2f, 0.2f, 1.0f },
                                { Vector3D(0.02f, 0.50f, 0.02f), Vector3D(0.0f, 0.35f, 0.0f), 0.2f, 1.0f, 0.2f, 1.0f },
                                { Vector3D(0.06f, 0.06f, 0.06f), Vector3D(0.0f, 0.62f, 0.0f), 0.2f, 1.0f, 0.2f, 1.0f },
                                { Vector3D(0.02f, 0.02f, 0.50f), Vector3D(0.0f, 0.0f, 0.35f), 0.2f, 0.4f, 1.0f, 1.0f },
                                { Vector3D(0.06f, 0.06f, 0.06f), Vector3D(0.0f, 0.0f, 0.62f), 0.2f, 0.4f, 1.0f, 1.0f },
                            };
                            renderHandles(ha, 6);
                        } else if (currentTool == ToolRotate) {
                            Handle ha[] = {
                                { Vector3D(0.35f, 0.08f, 0.08f), Vector3D(0.25f, 0.0f, 0.0f), 1.0f, 0.2f, 0.2f, 1.0f },
                                { Vector3D(0.08f, 0.35f, 0.08f), Vector3D(0.0f, 0.25f, 0.0f), 0.2f, 1.0f, 0.2f, 1.0f },
                                { Vector3D(0.08f, 0.08f, 0.35f), Vector3D(0.0f, 0.0f, 0.25f), 0.2f, 0.4f, 1.0f, 1.0f },
                            };
                            renderHandles(ha, 3);
                        } else if (currentTool == ToolScale) {
                            Handle ha[] = {
                                { Vector3D(0.10f, 0.10f, 0.10f), Vector3D(0.65f, 0.0f, 0.0f), 1.0f, 0.2f, 0.2f, 1.0f },
                                { Vector3D(0.10f, 0.10f, 0.10f), Vector3D(0.0f, 0.65f, 0.0f), 0.2f, 1.0f, 0.2f, 1.0f },
                                { Vector3D(0.10f, 0.10f, 0.10f), Vector3D(0.0f, 0.0f, 0.65f), 0.2f, 0.4f, 1.0f, 1.0f },
                                { Vector3D(0.06f, 0.06f, 0.06f), Vector3D(0.0f, 0.0f, 0.0f), 0.6f, 0.6f, 0.6f, 0.5f },
                            };
                            renderHandles(ha, 4);
                        }
                    }
 
                    // Render ImGui UI on top inside the render pass
                    ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, renderPass);
 
                    SDL_EndGPURenderPass(renderPass);
                }
            }
            // 5. Submit command buffer
            SDL_SubmitGPUCommandBuffer(cmd);
        }

        // Frame rate limiter check
        frameTime = SDL_GetTicks() - frameStartTicks;
        if (frameTime < FRAME_TARGET_TIME) {
            SDL_Delay((Uint32)(FRAME_TARGET_TIME - frameTime));
        }
    }

    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (depthTexture != nullptr) {
        SDL_ReleaseGPUTexture(pipeline.Device, depthTexture);
        depthTexture = nullptr;
    }

    pipeline.Cleanup();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
