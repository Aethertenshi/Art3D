#pragma once
#include <memory>
#include <vector>
#include <functional>
#include "core/GameObject.h"
#include "core/Vector3D.h"
#include "render/GpuPipeline.h"

struct GpuBufferDeleter {
    SDL_GPUDevice* Device;
    void operator()(SDL_GPUBuffer* buffer) const {
        if (Device && buffer) SDL_ReleaseGPUBuffer(Device, buffer);
    }
};

struct GpuTextureDeleter {
    SDL_GPUDevice* Device;
    void operator()(SDL_GPUTexture* tex) const {
        if (Device && tex) SDL_ReleaseGPUTexture(Device, tex);
    }
};

void DrawImportModal(bool& showImportModal,
                     std::vector<std::shared_ptr<GameObject>>& sceneObjects,
                     GpuPipeline& pipeline,
                     std::function<void(std::shared_ptr<GameObject>)> addObject,
                     GameObject*& selectedObject);
