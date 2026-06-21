#include "Application.h"

ImFont* g_MonoFont = nullptr;
#include "core/Matrix4x4.h"
#include "core/Vector3D.h"
#include "scene/LightSource.h"
#include "scene/Sun.h"
#include "scene/LogicScript.h"
#include "scene/SceneUtils.h"
#include "render/GpuTypes.h"
#include "script/ScriptManager.h"
#include "editor/ScriptEditor.h"
#include "editor/Toolbar.h"
#include "editor/ImportModal.h"
#include "editor/Autocomplete.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <iostream>
#include <fstream>
#include <cmath>

Application::Application()
    : m_Renderer(m_Pipeline) {}

Application::~Application() {
    Shutdown();
}

bool Application::Initialize() {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);

    auto cubeObj = std::make_shared<GameObject>("3D Cube", Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f));
    auto lightObj = std::make_shared<LightSource>("LightSource");
    auto sunObj = std::make_shared<Sun>("Sun");

    m_SceneObjects = { cubeObj, lightObj, sunObj };
    ScriptManager::Initialize();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    m_Window = SDL_CreateWindow("Art3DC++ Engine Viewport", 1920, 1080, 0);
    if (!m_Window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    if (!m_Pipeline.Initialize(m_Window)) {
        SDL_Log("Failed to initialize GPU pipeline!");
        SDL_DestroyWindow(m_Window);
        SDL_Quit();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    std::ifstream iniCheck("imgui.ini");
    if (iniCheck.good()) {
        iniCheck.close();
        ImGui::LoadIniSettingsFromDisk("imgui.ini");
    }
    io.IniFilename = "imgui.ini";

    std::string uiFontPath = ResolveAssetPath("ttf/GoogleSans-Regular.ttf");
    ImFont* defaultFont = io.Fonts->AddFontFromFileTTF(uiFontPath.c_str(), 15.0f);
    if (!defaultFont) io.Fonts->AddFontDefault();

    {
        ImFontConfig mergeCfg;
        mergeCfg.MergeMode = true;
        mergeCfg.GlyphOffset = ImVec2(0, 3);
        static const ImWchar lucideRanges[] = { 0xe000, 0xf000, 0 };
        io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/Lucide.ttf").c_str(), 15.0f, &mergeCfg, lucideRanges);
    }

    io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/GoogleSans-Bold.ttf").c_str(), 15.0f);
    {
        ImFontConfig mergeCfg;
        mergeCfg.MergeMode = true;
        mergeCfg.GlyphOffset = ImVec2(0, 3);
        static const ImWchar lucideRanges[] = { 0xe000, 0xf000, 0 };
        io.Fonts->AddFontFromFileTTF(ResolveAssetPath("ttf/Lucide.ttf").c_str(), 15.0f, &mergeCfg, lucideRanges);
    }

    std::string monoFontPath = ResolveAssetPath("ttf/JetBrainsMono-Regular.ttf");
    g_MonoFont = io.Fonts->AddFontFromFileTTF(monoFontPath.c_str(), 17.0f);

    ApplyTheme();

    ImGui_ImplSDL3_InitForSDLGPU(m_Window);

    ImGui_ImplSDLGPU3_InitInfo initInfo = {};
    initInfo.Device = m_Pipeline.Device;
    initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(m_Pipeline.Device, m_Window);
    initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&initInfo);

    m_Console.Initialize();
    SetupCallbacks();

    return true;
}

void Application::SetupCallbacks() {
    m_Explorer.SetInsertCallback([&](GameObject* parent, const std::string& type) {
        SaveHistory();
        std::shared_ptr<GameObject> newObj;
        if (type == "3D") {
            newObj = std::make_shared<GameObject>("3D Object", Vector3D(0, 0, 0), Vector3D(0, 0, 0), Vector3D(1, 1, 1));
        } else if (type == "Light") {
            newObj = std::make_shared<LightSource>("LightSource");
        } else if (type == "Script") {
            newObj = std::make_shared<LogicScript>("LogicScript");
        }
        if (newObj) {
            newObj->GpuDevice = m_Pipeline.Device;
            parent->AddChild(newObj);
            m_SelectedObject = newObj.get();
        }
    });
}

void Application::ApplyTheme() {
    ImGui::StyleColorsDark();
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
}

static bool s_resetDockspaceLayout = false;

void Application::SetupDockspace() {
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    static bool dockspace_initialized = false;

    if (!dockspace_initialized || s_resetDockspaceLayout) {
        dockspace_initialized = true;
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        bool hasLayout = (node != nullptr && node->ChildNodes[0] != nullptr);

        if (!hasLayout || s_resetDockspaceLayout) {
            s_resetDockspaceLayout = false;
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
}

void Application::SaveHistory() {
    m_UndoStack.push_back(CloneScene(m_SceneObjects));
    if (m_UndoStack.size() > 10) {
        m_UndoStack.erase(m_UndoStack.begin());
    }
}

void Application::AddObject(std::shared_ptr<GameObject> obj) {
    SaveHistory();
    m_SceneObjects.push_back(obj);
}

void Application::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I) && !io.WantTextInput) {
        m_ShowImportModal = true;
    }
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_CurrentTool = Renderer::ToolDrag;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) m_CurrentTool = Renderer::ToolMove;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) m_CurrentTool = Renderer::ToolRotate;
        if (ImGui::IsKeyPressed(ImGuiKey_4)) m_CurrentTool = Renderer::ToolScale;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.WantTextInput) {
        if (!m_UndoStack.empty()) {
            std::string selectedName = m_SelectedObject ? m_SelectedObject->Name : "";
            m_SceneObjects = m_UndoStack.back();
            m_UndoStack.pop_back();
            if (!selectedName.empty()) m_SelectedObject = FindSelectedInScene(m_SceneObjects, selectedName);
            else m_SelectedObject = nullptr;
        }
    }
    if (m_SelectedObject && ImGui::IsKeyPressed(ImGuiKey_Delete) && !io.WantTextInput) {
        SaveHistory();
        DeleteObject(m_SelectedObject, m_SceneObjects);
        m_SelectedObject = nullptr;
    }
}

void Application::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        bool intercepted = false;
        if (Autocomplete::g_autocompleteOpen) {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                SDL_Keycode key = event.key.key;
                if (key == SDLK_UP || key == SDLK_DOWN || key == SDLK_RETURN || key == SDLK_TAB || key == SDLK_ESCAPE) {
                    intercepted = true;
                    if (event.type == SDL_EVENT_KEY_DOWN) {
                        if (key == SDLK_UP) Autocomplete::g_autocompleteActiveIndex--;
                        else if (key == SDLK_DOWN) Autocomplete::g_autocompleteActiveIndex++;
                        else if (key == SDLK_RETURN || key == SDLK_TAB) Autocomplete::g_autocompleteSelectTriggered = true;
                        else if (key == SDLK_ESCAPE) Autocomplete::g_autocompleteCloseTriggered = true;
                    }
                }
            }
        }

        if (!intercepted) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (!ImGui::GetIO().WantTextInput) m_Camera.ProcessEvent(event, m_Window);
            else m_Camera.ReleaseMouse(m_Window);
        } else {
            if (ImGui::GetIO().WantTextInput) m_Camera.ReleaseMouse(m_Window);
        }
        if (event.type == SDL_EVENT_QUIT) m_Running = false;

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                int winW, winH;
                SDL_GetWindowSize(m_Window, &winW, &winH);
                float mouseX = event.button.x;
                float mouseY = event.button.y;

                float ndcX = (2.0f * mouseX) / (float)winW - 1.0f;
                float ndcY = 1.0f - (2.0f * mouseY) / (float)winH;
                float cosPitch = cosf(m_Camera.Pitch);
                float sinPitch = sinf(m_Camera.Pitch);
                float sinYaw = sinf(m_Camera.Yaw);
                float cosYaw = cosf(m_Camera.Yaw);
                float fwdX = sinYaw * cosPitch, fwdY = -sinPitch, fwdZ = cosYaw * cosPitch;
                float rgtX = cosYaw, rgtZ = -sinYaw;
                float upX = fwdY * rgtZ, upY = fwdZ * rgtX - fwdX * rgtZ, upZ = -fwdY * rgtX;
                float aspect = (float)winW / (float)winH;
                float tanHFov = tanf(45.0f * 3.14159265f / 180.0f * 0.5f);
                float sx = ndcX * tanHFov * aspect, sy = ndcY * tanHFov;
                float dirX = fwdX + rgtX*sx + upX*sy;
                float dirY = fwdY + upY*sy;
                float dirZ = fwdZ + rgtZ*sx + upZ*sy;
                float dlen = sqrtf(dirX*dirX + dirY*dirY + dirZ*dirZ);
                dirX /= dlen; dirY /= dlen; dirZ /= dlen;
                float origX = m_Camera.X, origY = m_Camera.Y, origZ = m_Camera.Z;

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

                m_DragAxis = -1;
                if (m_SelectedObject && m_SelectedObject->ClassName != "Sun" && m_CurrentTool == Renderer::ToolMove) {
                    Vector3D worldPos = m_SelectedObject->GetWorldPosition();
                    float px = worldPos.X, py = worldPos.Y, pz = worldPos.Z;
                    Vector3D worldScale = m_SelectedObject->GetWorldScale();
                    float hx = worldScale.X * 0.5f;
                    float hy = worldScale.Y * 0.5f;
                    float hz = worldScale.Z * 0.5f;

                    // Matches Renderer ToolMove: gap=0.06, shaftLen=0.60, coneH=0.16
                    struct AxisRegion { float cx,cy,cz, hx,hy,hz; int axis; };
                    float sMid = 0.36f, sHalf = 0.30f, sThin = 0.05f;
                    float cMid = 0.74f, cHalf = 0.10f;
                    AxisRegion regions[] = {
                        { px+hx+sMid, py, pz,  sHalf, sThin, sThin,  0 },
                        { px+hx+cMid, py, pz,  cHalf, cHalf, cHalf,  0 },
                        { px, py+hy+sMid, pz,  sThin, sHalf, sThin,  1 },
                        { px, py+hy+cMid, pz,  cHalf, cHalf, cHalf,  1 },
                        { px, py, pz+hz+sMid,  sThin, sThin, sHalf,  2 },
                        { px, py, pz+hz+cMid,  cHalf, cHalf, cHalf,  2 },
                    };
                    float bestT = 1e9f;
                    for (auto& r : regions) {
                        float t = rayAABB(r.cx, r.cy, r.cz, r.hx, r.hy, r.hz);
                        if (t >= 0.f && t < bestT) { bestT = t; m_DragAxis = r.axis; }
                    }
                    if (m_DragAxis >= 0) {
                        SaveHistory();
                        m_LastMX = mouseX; m_LastMY = mouseY;
                    }
                }
                else if (m_SelectedObject && m_SelectedObject->ClassName != "Sun" && m_CurrentTool == Renderer::ToolRotate) {
                    Vector3D worldPos = m_SelectedObject->GetWorldPosition();
                    float px = worldPos.X, py = worldPos.Y, pz = worldPos.Z;

                    // Matches Renderer ToolRotate: ringR=0.80, 18 dots, dotSize=0.035
                    float ringR = 0.80f;
                    int dots = 18;
                    float dotHalf = 0.05f;
                    float bestT = 1e9f;
                    for (int a = 0; a < 3; a++) {
                        for (int d = 0; d < dots; d++) {
                            float ang = (float)d / dots * 6.283185f;
                            float cx, cy, cz;
                            if (a == 0)      { cx = px;                   cy = py + cosf(ang)*ringR; cz = pz + sinf(ang)*ringR; }
                            else if (a == 1) { cx = px + cosf(ang)*ringR; cy = py;                   cz = pz + sinf(ang)*ringR; }
                            else             { cx = px + cosf(ang)*ringR; cy = py + sinf(ang)*ringR;  cz = pz; }
                            float t = rayAABB(cx, cy, cz, dotHalf, dotHalf, dotHalf);
                            if (t >= 0.f && t < bestT) { bestT = t; m_DragAxis = a; }
                        }
                    }
                    if (m_DragAxis >= 0) {
                        SaveHistory();
                        m_LastMX = mouseX; m_LastMY = mouseY;
                    }
                }
                else if (m_SelectedObject && m_SelectedObject->ClassName != "Sun" && m_CurrentTool == Renderer::ToolScale) {
                    Vector3D worldPos = m_SelectedObject->GetWorldPosition();
                    float px = worldPos.X, py = worldPos.Y, pz = worldPos.Z;
                    Vector3D worldScale = m_SelectedObject->GetWorldScale();
                    float hx = worldScale.X * 0.5f;
                    float hy = worldScale.Y * 0.5f;
                    float hz = worldScale.Z * 0.5f;

                    // Matches Renderer ToolScale: gap=0.06, endDist=0.75, endpoint radius=0.11
                    float endDist = 0.75f;
                    float endHalf = 0.11f;
                    float lineHalf = 0.025f;
                    float lineLen = endDist - 0.06f;
                    float lineMid = 0.06f + lineLen * 0.5f;

                    struct AxisRegion { float cx,cy,cz, hx,hy,hz; int axis; };
                    AxisRegion regions[] = {
                        { px+hx+lineMid, py, pz,  lineLen*0.5f, lineHalf, lineHalf,  0 },
                        { px+hx+endDist, py, pz,  endHalf, endHalf, endHalf,         0 },
                        { px, py+hy+lineMid, pz,  lineHalf, lineLen*0.5f, lineHalf,  1 },
                        { px, py+hy+endDist, pz,  endHalf, endHalf, endHalf,         1 },
                        { px, py, pz+hz+lineMid,  lineHalf, lineHalf, lineLen*0.5f,  2 },
                        { px, py, pz+hz+endDist,  endHalf, endHalf, endHalf,         2 },
                    };
                    float bestT = 1e9f;
                    for (auto& r : regions) {
                        float t = rayAABB(r.cx, r.cy, r.cz, r.hx, r.hy, r.hz);
                        if (t >= 0.f && t < bestT) { bestT = t; m_DragAxis = r.axis; }
                    }
                    if (m_DragAxis >= 0) {
                        SaveHistory();
                        m_LastMX = mouseX; m_LastMY = mouseY;
                    }
                }

                if (m_DragAxis < 0) {
                    m_SelectedObject = nullptr;
                    float closestT = 1e9f;
                    for (auto& obj : m_SceneObjects) {
                        if (!obj->Is3D) continue;
                        if (obj->ClassName == "Sun") continue;  // Sun not clickable in viewport
                        float hx = obj->Scale3D.X * 0.5f;
                        float hy = obj->Scale3D.Y * 0.5f;
                        float hz = obj->Scale3D.Z * 0.5f;
                        if (obj->ClassName == "LightSource") { hx *= 0.15f; hy *= 0.15f; hz *= 0.15f; }
                        Vector3D worldPos = GetFinalWorldCenter(obj.get());
                        float t = rayAABB(worldPos.X, worldPos.Y, worldPos.Z, hx, hy, hz);
                        if (t >= 0.f && t < closestT) { closestT = t; m_SelectedObject = obj.get(); }
                    }
                }
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
            m_DragAxis = -1;
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION && m_DragAxis >= 0 && m_SelectedObject) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                int winW, winH;
                SDL_GetWindowSize(m_Window, &winW, &winH);
                float curMX = event.motion.x, curMY = event.motion.y;
                float dmx = curMX - m_LastMX, dmy = curMY - m_LastMY;
                m_LastMX = curMX; m_LastMY = curMY;

                float cosPitch = cosf(m_Camera.Pitch);
                float sinPitch = sinf(m_Camera.Pitch);
                float sinYaw = sinf(m_Camera.Yaw);
                float cosYaw = cosf(m_Camera.Yaw);
                float fwdX = sinYaw * cosPitch, fwdY = -sinPitch, fwdZ = cosYaw * cosPitch;
                float rgtX = cosYaw, rgtZ = -sinYaw;
                float upX = fwdY * rgtZ, upY = fwdZ * rgtX - fwdX * rgtZ, upZ = -fwdY * rgtX;

                float axW[3] = { (m_DragAxis==0)?1.f:0.f, (m_DragAxis==1)?1.f:0.f, (m_DragAxis==2)?1.f:0.f };

                // Screen-space projection of the drag axis
                float screenAxX = axW[0]*rgtX + axW[2]*rgtZ;
                float screenAxY = -(axW[0]*upX + axW[1]*upY + axW[2]*upZ);
                float slen = sqrtf(screenAxX*screenAxX + screenAxY*screenAxY);
                if (slen > 0.001f) { screenAxX /= slen; screenAxY /= slen; }

                float dx = m_SelectedObject->Position3D.X - m_Camera.X;
                float dy = m_SelectedObject->Position3D.Y - m_Camera.Y;
                float dz = m_SelectedObject->Position3D.Z - m_Camera.Z;
                float depth = sqrtf(dx*dx + dy*dy + dz*dz);
                float aspect = (float)winW / (float)winH;
                float tanHFov = tanf(45.0f * 3.14159265f / 180.0f * 0.5f);
                float worldScale = depth * tanHFov * 2.0f / (float)winH;

                if (m_CurrentTool == Renderer::ToolMove) {
                    float move = (dmx * screenAxX + dmy * screenAxY) * worldScale;
                    if (m_DragAxis == 0) m_SelectedObject->Position3D.X += move;
                    if (m_DragAxis == 1) m_SelectedObject->Position3D.Y += move;
                    if (m_DragAxis == 2) m_SelectedObject->Position3D.Z += move;
                }
                else if (m_CurrentTool == Renderer::ToolScale) {
                    float move = (dmx * screenAxX + dmy * screenAxY) * worldScale;
                    if (m_DragAxis == 0) m_SelectedObject->Scale3D.X = std::max(0.01f, m_SelectedObject->Scale3D.X + move);
                    if (m_DragAxis == 1) m_SelectedObject->Scale3D.Y = std::max(0.01f, m_SelectedObject->Scale3D.Y + move);
                    if (m_DragAxis == 2) m_SelectedObject->Scale3D.Z = std::max(0.01f, m_SelectedObject->Scale3D.Z + move);
                }
                else if (m_CurrentTool == Renderer::ToolRotate) {
                    // Tangent = perpendicular to screen-space axis direction
                    float tanX = -screenAxY;
                    float tanY =  screenAxX;
                    // Project mouse delta onto tangent, convert to degrees
                    float rotateDeg = -(dmx * tanX + dmy * tanY) * 0.8f;
                    if (m_DragAxis == 0) m_SelectedObject->Rotation3D.X += rotateDeg;
                    if (m_DragAxis == 1) m_SelectedObject->Rotation3D.Y += rotateDeg;
                    if (m_DragAxis == 2) m_SelectedObject->Rotation3D.Z += rotateDeg;
                }
            }
        }
    }
}

void Application::RenderFrame() {
    if (!ImGui::GetIO().WantTextInput && !ImGui::GetIO().WantCaptureKeyboard) {
        m_Camera.Update();
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    HandleShortcuts();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit", "Alt+F4")) m_Running = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_UndoStack.empty())) {
                std::string selectedName = m_SelectedObject ? m_SelectedObject->Name : "";
                if (!m_UndoStack.empty()) {
                    m_SceneObjects = m_UndoStack.back();
                    m_UndoStack.pop_back();
                    if (!selectedName.empty()) m_SelectedObject = FindSelectedInScene(m_SceneObjects, selectedName);
                    else m_SelectedObject = nullptr;
                }
            }
            if (ImGui::MenuItem("Delete Selected", "Del", false, m_SelectedObject != nullptr)) {
                SaveHistory();
                DeleteObject(m_SelectedObject, m_SceneObjects);
                m_SelectedObject = nullptr;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import Object...", "Ctrl+I")) m_ShowImportModal = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Skybox", nullptr, &m_ShowSkybox);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Reset Layout")) {
                s_resetDockspaceLayout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    DrawImportModal(m_ShowImportModal, m_SceneObjects, m_Pipeline,
        [&](std::shared_ptr<GameObject> obj) { AddObject(obj); },
        m_SelectedObject);

    SetupDockspace();

    m_Explorer.Draw(m_SceneObjects, m_SelectedObject);

    bool saveHistoryFromProps = false;
    m_Properties.Draw(m_SelectedObject, saveHistoryFromProps);
    if (saveHistoryFromProps) SaveHistory();

    m_Console.Draw();

    SyncOpenScripts(m_SceneObjects);
    DrawScriptEditor(m_SelectedObject);

    auto gizmoTool = static_cast<Toolbar::GizmoTool>(m_CurrentTool);
    Toolbar::Draw(gizmoTool);
    m_CurrentTool = static_cast<Renderer::GizmoTool>(gizmoTool);

    m_Renderer.Render2DObjects(m_SceneObjects);

    ImGui::Render();

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Pipeline.Device);
    if (cmd) {
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);

        SDL_GPUTexture* swapchainTexture = nullptr;
        Uint32 w = 0, h = 0;
        if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, m_Window, &swapchainTexture, &w, &h) && swapchainTexture) {
            if (m_DepthTexture == nullptr || m_DepthWidth != w || m_DepthHeight != h) {
                if (m_DepthTexture) SDL_ReleaseGPUTexture(m_Pipeline.Device, m_DepthTexture);
                SDL_GPUTextureCreateInfo depthInfo = {};
                depthInfo.type = SDL_GPU_TEXTURETYPE_2D;
                depthInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
                depthInfo.width = w;
                depthInfo.height = h;
                depthInfo.layer_count_or_depth = 1;
                depthInfo.num_levels = 1;
                depthInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
                m_DepthTexture = SDL_CreateGPUTexture(m_Pipeline.Device, &depthInfo);
                m_DepthWidth = w;
                m_DepthHeight = h;
            }

            SDL_GPUColorTargetInfo colorTargetInfo = {};
            colorTargetInfo.texture = swapchainTexture;
            colorTargetInfo.clear_color = { 0.53f, 0.81f, 0.92f, 1.0f };
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo depthTargetInfo = {};
            depthTargetInfo.texture = m_DepthTexture;
            depthTargetInfo.clear_depth = 1.0f;
            depthTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            depthTargetInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTargetInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depthTargetInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            depthTargetInfo.cycle = true;

            // Collect lights and run shadow pass BEFORE main render pass
            float aspect = (h > 0) ? ((float)w / (float)h) : 1.0f;
            Matrix4x4 proj = Matrix4x4::Perspective(45.0f, aspect, 0.1f, 100.0f);
            Matrix4x4 view = m_Camera.GetViewMatrix();

            LightUBO lightData = m_Renderer.CollectLights(m_SceneObjects);
            lightData.HasShadow = 0;
            lightData.LightViewProj = Matrix4x4::Identity();

            // Find first shadow-casting directional/spot light or sun and render shadow pass
            for (auto& obj : m_SceneObjects) {
                if ((obj->ClassName == "LightSource" || obj->ClassName == "Sun") && obj->Is3D) {
                    LightSource* light = static_cast<LightSource*>(obj.get());
                    if (light->CastShadow &&
                        (light->Type == LightType::Directional || light->Type == LightType::Spot)) {
                        Matrix4x4 lvp = m_Renderer.RenderShadowPass(cmd, light, m_SceneObjects);
                        lightData.LightViewProj = lvp;
                        lightData.HasShadow = 1;
                        break;
                    }
                }
            }

            SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, &depthTargetInfo);
            if (renderPass) {
                SDL_GPUViewport viewport = {};
                viewport.min_depth = 0.0f; viewport.max_depth = 1.0f;
                viewport.x = 0.0f; viewport.y = 0.0f;
                viewport.w = (float)w; viewport.h = (float)h;
                SDL_SetGPUViewport(renderPass, &viewport);

                // Render skybox first (no depth write, always behind scene)
                if (m_ShowSkybox) {
                    m_Renderer.RenderSky(renderPass, cmd, m_Camera.X, m_Camera.Y, m_Camera.Z, view, proj);
                }

                SDL_BindGPUGraphicsPipeline(renderPass, m_Pipeline.Pipeline);

                SDL_GPUBufferBinding vertexBinding = {};
                vertexBinding.buffer = m_Pipeline.VertexBuffer;
                vertexBinding.offset = 0;
                SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);
                SDL_GPUBufferBinding indexBinding = {};
                indexBinding.buffer = m_Pipeline.IndexBuffer;
                indexBinding.offset = 0;
                SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

                // Bind shadow map (s0) + default diffuse texture (s1) for main pipeline
                SDL_GPUTextureSamplerBinding samplerBindings[2] = {};
                samplerBindings[0].texture = m_Pipeline.ShadowMap;
                samplerBindings[0].sampler = m_Pipeline.ShadowSampler;
                samplerBindings[1].texture = m_Pipeline.DefaultTexture;
                samplerBindings[1].sampler = m_Pipeline.DefaultSampler;
                SDL_BindGPUFragmentSamplers(renderPass, 0, samplerBindings, 2);

                // Push light UBO once per frame (fixes the per-object push waste bug)
                SDL_PushGPUFragmentUniformData(cmd, 0, &lightData, sizeof(lightData));

                m_Renderer.RenderGrid(renderPass, cmd, view, proj, lightData);
                m_Renderer.Render3DObjects(renderPass, cmd, m_SceneObjects, view, proj, lightData);

                // Render visual sun in the sky (after scene, before gizmo)
                for (auto& obj : m_SceneObjects) {
                    if (obj->ClassName == "Sun") {
                        Sun* sun = static_cast<Sun*>(obj.get());
                        sun->UpdateFromTime();
                        m_Renderer.RenderSun(renderPass, cmd, sun, view, proj);
                        break;
                    }
                }

                if (m_SelectedObject && m_SelectedObject->ClassName != "Sun") {
                    m_Renderer.RenderGizmo(renderPass, cmd, m_SelectedObject, m_CurrentTool, view, proj, lightData);
                }
                ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, renderPass);
                SDL_EndGPURenderPass(renderPass);
            }
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }
}

void Application::MainLoop() {
    while (m_Running) {
        Uint64 frameStartTicks = SDL_GetTicks();
        ProcessEvents();
        RenderFrame();
        Uint64 frameTime = SDL_GetTicks() - frameStartTicks;
        if (frameTime < FRAME_TARGET_TIME) {
            SDL_Delay((Uint32)(FRAME_TARGET_TIME - frameTime));
        }
    }
}

void Application::Shutdown() {
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_DepthTexture) {
        SDL_ReleaseGPUTexture(m_Pipeline.Device, m_DepthTexture);
        m_DepthTexture = nullptr;
    }

    m_Pipeline.Cleanup();
    if (m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();
}

int Application::Run() {
    if (!Initialize()) return 1;
    MainLoop();
    return 0;
}
