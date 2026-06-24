#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <memory>
#include <vector>
#include <functional>
#include "core/GameObject.h"
#include "camera/CameraController.h"
#include "render/GpuPipeline.h"
#include "render/Renderer.h"
#include "editor/ExplorerWindow.h"
#include "editor/PropertiesWindow.h"
#include "editor/ConsoleWindow.h"
#include "editor/AssetBrowser.h"
#include "audio/AudioPlayer.h"

class Application {
public:
    Application();
    ~Application();
    int Run();

private:
    bool Initialize();
    void Shutdown();
    void MainLoop();
    void ApplyTheme();
    void SetupDockspace();
    void ProcessEvents();
    void RenderFrame();
    void HandleShortcuts();
    void SetupCallbacks();

    SDL_Window* m_Window = nullptr;
    GpuPipeline m_Pipeline;
    Renderer m_Renderer;
    CameraController m_Camera;
    ExplorerWindow m_Explorer;
    PropertiesWindow m_Properties;
    ConsoleWindow m_Console;
    AssetBrowser m_AssetBrowser;
    AudioPlayer m_AudioPlayer;
    ma_engine* m_AudioEngine = nullptr;

    std::vector<std::shared_ptr<GameObject>> m_SceneObjects;
    std::vector<std::vector<std::shared_ptr<GameObject>>> m_UndoStack;
    GameObject* m_SelectedObject = nullptr;

    Renderer::GizmoTool m_CurrentTool = Renderer::ToolDrag;
    bool m_ShowImportModal = false;
    bool m_Running = true;
    bool m_ShowSkybox = true;

    SDL_GPUTexture* m_DepthTexture = nullptr;
    Uint32 m_DepthWidth = 0;
    Uint32 m_DepthHeight = 0;

    int m_DragAxis = -1;
    float m_LastMX = 0.f;
    float m_LastMY = 0.f;

    const int TARGET_FPS = 120;
    const Uint64 FRAME_TARGET_TIME = 1000 / TARGET_FPS;

    void SaveHistory();
    void AddObject(std::shared_ptr<GameObject> obj);
    void HandleAssetDrop(const std::string& path);
};
