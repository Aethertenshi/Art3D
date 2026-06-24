#pragma once
#include <string>
#include <vector>
#include <SDL3/SDL_gpu.h>
#include "core/Vector3D.h"
#include "render/Vertex.h"

struct GLTFSubMesh {
    uint32_t IndexStart;
    uint32_t IndexCount;
    SDL_GPUTexture* Texture;  // nullptr = use default white
};

bool LoadGLTF(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter,
              SDL_GPUDevice* device, std::vector<GLTFSubMesh>& outSubMeshes);
