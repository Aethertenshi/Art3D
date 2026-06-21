#pragma once
#include <string>
#include <vector>
#include "core/Vector3D.h"
#include "render/GpuPipeline.h"

bool LoadObj(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter);
