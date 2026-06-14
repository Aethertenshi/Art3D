#include <SDL3/SDL.h>
#include <iostream>
#include <memory>
#include <vector>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

import Position2D;
import Size2D;
import BaseDrawables;
import Frame;
import GameObject;
import ExplorerWindow;
import PropertiesWindow;

int main() {
    // Create hierarchy of GameObjects
    auto rootObj = std::make_shared<GameObject>("SomeObject", Position2D(1, 1, 100, 100), Size2D(1, 1, 400, 300));
    auto childObj = std::make_shared<GameObject>("Object2", Position2D(1, 1, 120, 120), Size2D(1, 1, 200, 150));
    auto subChildObj = std::make_shared<GameObject>("Object3", Position2D(1, 1, 140, 140), Size2D(1, 1, 100, 80));
    
    childObj->AddChild(subChildObj);
    rootObj->AddChild(childObj);

    std::vector<std::shared_ptr<GameObject>> sceneObjects = { rootObj };
    GameObject* selectedObject = nullptr;

    // Interface Windows
    ExplorerWindow explorer;
    PropertiesWindow properties;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Art3DC++ Engine Viewport", 1280, 720, 0);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable docking branch features

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // Frame rate limiter variables
    const int TARGET_FPS = 120;
    const Uint64 FRAME_TARGET_TIME = 1000 / TARGET_FPS;
    Uint64 frameStartTicks;
    Uint64 frameTime;

    bool running = true;
    SDL_Event event;
    while (running) {
        frameStartTicks = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Create fullscreen Dockspace over the viewport (translucent center for scene rendering)
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

        // Render ImGui Interface Windows
        explorer.Draw(sceneObjects, selectedObject);
        properties.Draw(selectedObject);

        // Rendering Scene Viewport
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255); // Dark gray background
        SDL_RenderClear(renderer);

        // Simple lambda to recursively draw GameObjects in viewport
        auto drawObject = [&](auto& self, GameObject* obj) -> void {
            SDL_FRect rect = {
                (float)obj->Position.OffsetX,
                (float)obj->Position.OffsetY,
                (float)obj->Size.OffsetX,
                (float)obj->Size.OffsetY
            };
            
            // Draw selected object green, others white
            if (selectedObject == obj) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            SDL_RenderRect(renderer, &rect);

            for (auto& child : obj->Children) {
                self(self, child.get());
            }
        };

        // Render the hierarchy
        for (auto& obj : sceneObjects) {
            drawObject(drawObject, obj.get());
        }

        // Render ImGui Draw Data on top
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        
        SDL_RenderPresent(renderer);

        // Frame rate limiter check
        frameTime = SDL_GetTicks() - frameStartTicks;
        if (frameTime < FRAME_TARGET_TIME) {
            SDL_Delay((Uint32)(FRAME_TARGET_TIME - frameTime));
        }
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
