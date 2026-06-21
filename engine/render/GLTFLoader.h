#pragma once
#include <string>
#include <vector>
#include <SDL3/SDL_gpu.h>
#include "core/Vector3D.h"
#include "render/Vertex.h"

bool LoadGLTF(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter,
              SDL_GPUDevice* device, SDL_GPUTexture** outTexture);
