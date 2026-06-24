#define _CRT_SECURE_NO_WARNINGS

#include "GLTFLoader.h"
#include <SDL3_image/SDL_image.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

static void ComputeFlatNormal(Vertex& v0, Vertex& v1, Vertex& v2)
{
    float ex1 = v1.x - v0.x, ey1 = v1.y - v0.y, ez1 = v1.z - v0.z;
    float ex2 = v2.x - v0.x, ey2 = v2.y - v0.y, ez2 = v2.z - v0.z;
    float nx = ey1 * ez2 - ez1 * ey2;
    float ny = ez1 * ex2 - ex1 * ez2;
    float nz = ex1 * ey2 - ey1 * ex2;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 1e-6f) { nx /= len; ny /= len; nz /= len; }
    else { nx = 0.0f; ny = 1.0f; nz = 0.0f; }
    v0.nx = v1.nx = v2.nx = nx;
    v0.ny = v1.ny = v2.ny = ny;
    v0.nz = v1.nz = v2.nz = nz;
}

static SDL_GPUTexture* LoadMaterialTexture(SDL_GPUDevice* device, const cgltf_material* material, const std::string& filepath)
{
    if (!material || !device) return nullptr;

    const cgltf_texture_view* texView = nullptr;
    if (material->has_pbr_metallic_roughness && material->pbr_metallic_roughness.base_color_texture.texture)
        texView = &material->pbr_metallic_roughness.base_color_texture;
    if (!texView && material->emissive_texture.texture)
        texView = &material->emissive_texture;
    if (!texView && material->normal_texture.texture)
        texView = &material->normal_texture;
    if (!texView && material->occlusion_texture.texture)
        texView = &material->occlusion_texture;

    if (!texView || !texView->texture || !texView->texture->image) return nullptr;

    const cgltf_image& image = *texView->texture->image;
    if (!image.uri) return nullptr;

    std::string texPath = image.uri;
    std::string resolvedPath = texPath;
    std::filesystem::path gltfDir = std::filesystem::path(filepath).parent_path();
    if (!gltfDir.empty()) {
        resolvedPath = (gltfDir / texPath).string();
    }
    if (!std::filesystem::exists(resolvedPath)) {
        resolvedPath = texPath;
    }
    if (!std::filesystem::exists(resolvedPath)) {
        std::cerr << "glTF texture file not found: " << resolvedPath << std::endl;
        return nullptr;
    }

    SDL_GPUCommandBuffer* texCmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* texCopy = SDL_BeginGPUCopyPass(texCmd);
    int imgW = 0, imgH = 0;
    SDL_GPUTexture* loadedTex = IMG_LoadGPUTexture(device, texCopy, resolvedPath.c_str(), &imgW, &imgH);
    SDL_EndGPUCopyPass(texCopy);
    SDL_SubmitGPUCommandBuffer(texCmd);
    if (loadedTex) {
        std::cout << "Loaded glTF texture: " << resolvedPath << " (" << imgW << "x" << imgH << ")" << std::endl;
    } else {
        std::cerr << "Failed to load glTF texture: " << resolvedPath << std::endl;
    }
    return loadedTex;
}

bool LoadGLTF(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter,
              SDL_GPUDevice* device, std::vector<GLTFSubMesh>& outSubMeshes)
{
    outSubMeshes.clear();

    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "Failed to parse glTF file: " << filepath << " (error " << result << ")" << std::endl;
        return false;
    }

    {
        result = cgltf_load_buffers(&options, data, filepath.c_str());
        if (result != cgltf_result_success) {
            std::cerr << "Failed to load glTF buffers for: " << filepath << " (error code " << result << ")\n"
                      << "  Make sure any external .bin files and textures referenced by the .gltf are present.\n"
                      << "  For a .glb file, the data is embedded and no extra files are needed.\n";
            cgltf_free(data);
            return false;
        }
    }

    // Track per-primitive index ranges and material pointers
    struct PrimitiveInfo {
        uint32_t IndexStart;
        uint32_t IndexCount;
        const cgltf_material* Material;
    };
    std::vector<PrimitiveInfo> primitives;

    // Walk all meshes, flatten all triangle primitives into one vertex/index array
    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
    {
        const cgltf_mesh& mesh = data->meshes[mi];

        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
        {
            const cgltf_primitive& prim = mesh.primitives[pi];

            if (prim.type != cgltf_primitive_type_triangles)
                continue;

            const cgltf_accessor* posAccessor = nullptr;
            const cgltf_accessor* normAccessor = nullptr;
            const cgltf_accessor* uvAccessor = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
            {
                if (prim.attributes[ai].type == cgltf_attribute_type_position)
                    posAccessor = prim.attributes[ai].data;
                else if (prim.attributes[ai].type == cgltf_attribute_type_normal)
                    normAccessor = prim.attributes[ai].data;
                else if (prim.attributes[ai].type == cgltf_attribute_type_texcoord)
                    uvAccessor = prim.attributes[ai].data;
            }

            if (!posAccessor)
                continue;

            uint32_t baseVertex = (uint32_t)outVertices.size();

            // Read positions
            cgltf_size vertCount = posAccessor->count;
            std::vector<float> posData(vertCount * 3);
            cgltf_accessor_unpack_floats(posAccessor, posData.data(), (cgltf_size)posData.size());

            // Read normals if present
            std::vector<float> normData;
            bool hasNormals = false;
            if (normAccessor && normAccessor->count >= vertCount)
            {
                normData.resize(vertCount * 3);
                cgltf_accessor_unpack_floats(normAccessor, normData.data(), (cgltf_size)normData.size());
                hasNormals = true;
            }

            // Read UVs if present
            std::vector<float> uvData;
            bool hasUVs = false;
            if (uvAccessor && uvAccessor->count >= vertCount)
            {
                uvData.resize(vertCount * 2);
                cgltf_accessor_unpack_floats(uvAccessor, uvData.data(), (cgltf_size)uvData.size());
                hasUVs = true;
            }

            // Build vertices
            for (cgltf_size vi = 0; vi < vertCount; ++vi)
            {
                Vertex v = {};
                v.x = posData[vi * 3 + 0];
                v.y = posData[vi * 3 + 1];
                v.z = posData[vi * 3 + 2];
                v.r = 0.8f; v.g = 0.8f; v.b = 0.8f; v.a = 1.0f;
                v.u = 0.0f; v.v = 0.0f;

                if (hasNormals)
                {
                    v.nx = normData[vi * 3 + 0];
                    v.ny = normData[vi * 3 + 1];
                    v.nz = normData[vi * 3 + 2];
                }
                else
                {
                    v.nx = 0.0f; v.ny = 0.0f; v.nz = 0.0f;
                }

                if (hasUVs)
                {
                    v.u = uvData[vi * 2 + 0];
                    v.v = uvData[vi * 2 + 1];
                }

                outVertices.push_back(v);
            }

            // Record index start before reading indices
            uint32_t idxStart = (uint32_t)outIndices.size();

            // Read indices
            if (prim.indices)
            {
                cgltf_size idxCount = prim.indices->count;
                cgltf_size compSize = cgltf_component_size(prim.indices->component_type);
                cgltf_size stride = prim.indices->stride ? prim.indices->stride : compSize;
                const uint8_t* idxPtr = (const uint8_t*)(prim.indices->buffer_view->buffer->data)
                                        + prim.indices->buffer_view->offset
                                        + prim.indices->offset;

                for (cgltf_size ii = 0; ii < idxCount; ++ii)
                {
                    uint32_t idx = 0;
                    if (prim.indices->component_type == cgltf_component_type_r_32u)
                        idx = *(const uint32_t*)(idxPtr + ii * stride);
                    else if (prim.indices->component_type == cgltf_component_type_r_16u)
                        idx = *(const uint16_t*)(idxPtr + ii * stride);
                    else if (prim.indices->component_type == cgltf_component_type_r_8u)
                        idx = *(const uint8_t*)(idxPtr + ii * stride);
                    else
                        continue;

                    outIndices.push_back(baseVertex + idx);
                }
            }
            else
            {
                for (cgltf_size ii = 0; ii < vertCount; ++ii)
                    outIndices.push_back(baseVertex + (uint32_t)ii);
            }

            // Record this primitive's range and material
            PrimitiveInfo piInfo;
            piInfo.IndexStart = idxStart;
            piInfo.IndexCount = (uint32_t)(outIndices.size() - idxStart);
            piInfo.Material = prim.material;
            primitives.push_back(piInfo);
        }
    }

    // Build base sub-mesh entries (all with null textures)
    for (auto& pi : primitives)
    {
        GLTFSubMesh sm;
        sm.IndexStart = pi.IndexStart;
        sm.IndexCount = pi.IndexCount;
        sm.Texture = nullptr;
        outSubMeshes.push_back(sm);
    }

    // Load textures for unique materials and assign to sub-meshes
    if (device)
    {
        std::unordered_map<const cgltf_material*, SDL_GPUTexture*> matToTexture;
        for (size_t i = 0; i < primitives.size(); ++i)
        {
            const cgltf_material* mat = primitives[i].Material;
            if (!mat) continue;
            if (!matToTexture.count(mat))
            {
                matToTexture[mat] = LoadMaterialTexture(device, mat, filepath);
            }
            outSubMeshes[i].Texture = matToTexture[mat];
        }
    }

    cgltf_free(data);

    if (outVertices.empty())
    {
        std::cerr << "No triangle mesh data found in: " << filepath << std::endl;
        return false;
    }

    // Compute flat normals for any vertices with missing normals
    for (size_t i = 0; i + 2 < outIndices.size(); i += 3)
    {
        Vertex& v0 = outVertices[outIndices[i]];
        Vertex& v1 = outVertices[outIndices[i + 1]];
        Vertex& v2 = outVertices[outIndices[i + 2]];

        bool missingNormals =
            (v0.nx == 0.0f && v0.ny == 0.0f && v0.nz == 0.0f) &&
            (v1.nx == 0.0f && v1.ny == 0.0f && v1.nz == 0.0f) &&
            (v2.nx == 0.0f && v2.ny == 0.0f && v2.nz == 0.0f);

        if (missingNormals)
            ComputeFlatNormal(v0, v1, v2);
    }

    // Compute bounding box and center
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;

    for (const auto& v : outVertices)
    {
        if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
        if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
    }

    outCenter.X = (minX + maxX) * 0.5f;
    outCenter.Y = (minY + maxY) * 0.5f;
    outCenter.Z = (minZ + maxZ) * 0.5f;

    for (auto& v : outVertices)
    {
        v.x -= outCenter.X;
        v.y -= outCenter.Y;
        v.z -= outCenter.Z;
    }

    return true;
}
