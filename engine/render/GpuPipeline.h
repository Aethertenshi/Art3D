#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <filesystem>
#include "core/Matrix4x4.h"
#include "Vertex.h"
#include "MeshBuilder.h"

inline std::string ResolveAssetPath(const std::string& relativePath) {
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }
    std::string parentPath = "../" + relativePath;
    if (std::filesystem::exists(parentPath)) {
        return parentPath;
    }
    std::string grandparentPath = "../../" + relativePath;
    if (std::filesystem::exists(grandparentPath)) {
        return grandparentPath;
    }
    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::string base(basePath);
        std::string resolved = base + relativePath;
        if (std::filesystem::exists(resolved)) return resolved;
        resolved = base + "../" + relativePath;
        if (std::filesystem::exists(resolved)) return resolved;
        resolved = base + "../../" + relativePath;
        if (std::filesystem::exists(resolved)) return resolved;
    }
    return relativePath;
}

class GpuPipeline
{
public:
    SDL_GPUDevice* Device = nullptr;
    SDL_GPUGraphicsPipeline* Pipeline = nullptr;
    SDL_GPUBuffer* VertexBuffer = nullptr;
    SDL_GPUBuffer* IndexBuffer = nullptr;
    SDL_GPUBuffer* GridBuffer = nullptr;
    SDL_GPUBuffer* GridIndexBuffer = nullptr;
    int GridIndexCount = 0;

    // Gizmo shape buffers
    SDL_GPUBuffer* ConeVB = nullptr;
    SDL_GPUBuffer* ConeIB = nullptr;
    uint32_t ConeIndexCount = 0;
    SDL_GPUBuffer* CylinderVB = nullptr;
    SDL_GPUBuffer* CylinderIB = nullptr;
    uint32_t CylinderIndexCount = 0;
    SDL_GPUBuffer* SphereVB = nullptr;
    SDL_GPUBuffer* SphereIB = nullptr;
    uint32_t SphereIndexCount = 0;
    SDL_GPUBuffer* WedgeVB = nullptr;
    SDL_GPUBuffer* WedgeIB = nullptr;
    uint32_t WedgeIndexCount = 0;

    // Shadow mapping resources
    SDL_GPUTexture* ShadowMap = nullptr;
    SDL_GPUSampler* ShadowSampler = nullptr;
    SDL_GPUGraphicsPipeline* ShadowPipeline = nullptr;

    // Skybox resources
    SDL_GPUTexture* SkyTexture = nullptr;
    SDL_GPUSampler* SkySampler = nullptr;
    SDL_GPUGraphicsPipeline* SkyPipeline = nullptr;

    // Default 1x1 white texture for untextured objects
    SDL_GPUTexture* DefaultTexture = nullptr;
    SDL_GPUSampler* DefaultSampler = nullptr;

    GpuPipeline() {}

    ~GpuPipeline() {
        Cleanup();
    }

    void Cleanup() {
        if (SkyPipeline) { SDL_ReleaseGPUGraphicsPipeline(Device, SkyPipeline); SkyPipeline = nullptr; }
        if (SkyTexture) { SDL_ReleaseGPUTexture(Device, SkyTexture); SkyTexture = nullptr; }
        if (SkySampler) { SDL_ReleaseGPUSampler(Device, SkySampler); SkySampler = nullptr; }
        if (DefaultTexture) { SDL_ReleaseGPUTexture(Device, DefaultTexture); DefaultTexture = nullptr; }
        if (DefaultSampler) { SDL_ReleaseGPUSampler(Device, DefaultSampler); DefaultSampler = nullptr; }
        if (ShadowPipeline) { SDL_ReleaseGPUGraphicsPipeline(Device, ShadowPipeline); ShadowPipeline = nullptr; }
        if (ShadowMap) { SDL_ReleaseGPUTexture(Device, ShadowMap); ShadowMap = nullptr; }
        if (ShadowSampler) { SDL_ReleaseGPUSampler(Device, ShadowSampler); ShadowSampler = nullptr; }
        if (ConeVB) { SDL_ReleaseGPUBuffer(Device, ConeVB); ConeVB = nullptr; }
        if (ConeIB) { SDL_ReleaseGPUBuffer(Device, ConeIB); ConeIB = nullptr; }
        if (CylinderVB) { SDL_ReleaseGPUBuffer(Device, CylinderVB); CylinderVB = nullptr; }
        if (CylinderIB) { SDL_ReleaseGPUBuffer(Device, CylinderIB); CylinderIB = nullptr; }
        if (SphereVB) { SDL_ReleaseGPUBuffer(Device, SphereVB); SphereVB = nullptr; }
        if (SphereIB) { SDL_ReleaseGPUBuffer(Device, SphereIB); SphereIB = nullptr; }
        if (WedgeVB) { SDL_ReleaseGPUBuffer(Device, WedgeVB); WedgeVB = nullptr; }
        if (WedgeIB) { SDL_ReleaseGPUBuffer(Device, WedgeIB); WedgeIB = nullptr; }
        if (GridBuffer) { SDL_ReleaseGPUBuffer(Device, GridBuffer); GridBuffer = nullptr; }
        if (GridIndexBuffer) { SDL_ReleaseGPUBuffer(Device, GridIndexBuffer); GridIndexBuffer = nullptr; }
        if (VertexBuffer) { SDL_ReleaseGPUBuffer(Device, VertexBuffer); VertexBuffer = nullptr; }
        if (IndexBuffer) { SDL_ReleaseGPUBuffer(Device, IndexBuffer); IndexBuffer = nullptr; }
        if (Pipeline) { SDL_ReleaseGPUGraphicsPipeline(Device, Pipeline); Pipeline = nullptr; }
        if (Device) { SDL_DestroyGPUDevice(Device); Device = nullptr; }
    }

    SDL_GPUBuffer* CreateAndUploadBuffer(const void* data, Uint32 size, SDL_GPUBufferUsageFlags usage) {
        SDL_GPUBufferCreateInfo bufferInfo = {};
        bufferInfo.usage = usage;
        bufferInfo.size = size;
        SDL_GPUBuffer* destBuffer = SDL_CreateGPUBuffer(Device, &bufferInfo);
        if (!destBuffer) {
            std::cerr << "Failed to create GPU Buffer: " << SDL_GetError() << std::endl;
            return nullptr;
        }

        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = size;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(Device, &transferInfo);
        if (!transferBuffer) {
            std::cerr << "Failed to create transfer buffer: " << SDL_GetError() << std::endl;
            SDL_ReleaseGPUBuffer(Device, destBuffer);
            return nullptr;
        }

        void* map = SDL_MapGPUTransferBuffer(Device, transferBuffer, false);
        std::memcpy(map, data, size);
        SDL_UnmapGPUTransferBuffer(Device, transferBuffer);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(Device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);

        SDL_GPUTransferBufferLocation srcLocation = {};
        srcLocation.transfer_buffer = transferBuffer;
        srcLocation.offset = 0;

        SDL_GPUBufferRegion dstRegion = {};
        dstRegion.buffer = destBuffer;
        dstRegion.offset = 0;
        dstRegion.size = size;

        SDL_UploadToGPUBuffer(copy, &srcLocation, &dstRegion, false);
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);

        return destBuffer;
    }

    bool Initialize(SDL_Window* window) {
        SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC;
        Device = SDL_CreateGPUDevice(formats, true, nullptr);
        if (!Device) {
            std::cerr << "Failed to create GPU Device: " << SDL_GetError() << std::endl;
            return false;
        }

        std::cout << "GPU Device created successfully." << std::endl;
        std::cout << "GPU Driver Name: " << SDL_GetGPUDeviceDriver(Device) << std::endl;

        SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(Device);
        std::cout << "Supported Shader Formats by Device: ";
        if (supported & SDL_GPU_SHADERFORMAT_DXBC) std::cout << "DXBC ";
        if (supported & SDL_GPU_SHADERFORMAT_DXIL) std::cout << "DXIL ";
        if (supported & SDL_GPU_SHADERFORMAT_SPIRV) std::cout << "SPIRV ";
        std::cout << std::endl;

        if (!SDL_ClaimWindowForGPUDevice(Device, window)) {
            std::cerr << "Failed to claim window: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(Device, window);
        std::cout << "Swapchain Texture Format: " << (int)swapchainFormat << std::endl;

        size_t vsSize, psSize;
        std::string vsPath = ResolveAssetPath("engine/shaders/shader.vs.dxil");
        std::string psPath = ResolveAssetPath("engine/shaders/shader.ps.dxil");
        void* vsCode = SDL_LoadFile(vsPath.c_str(), &vsSize);
        void* psCode = SDL_LoadFile(psPath.c_str(), &psSize);

        if (!vsCode || !psCode) {
            std::cerr << "Failed to load shader binaries from resolved paths: " << vsPath << " or " << psPath << std::endl;
            return false;
        }

        SDL_GPUShaderCreateInfo vsInfo = {};
        vsInfo.code_size = vsSize;
        vsInfo.code = (const Uint8*)vsCode;
        vsInfo.entrypoint = "VSMain";
        vsInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vsInfo.num_uniform_buffers = 1;
        SDL_GPUShader* vsShader = SDL_CreateGPUShader(Device, &vsInfo);
        SDL_free(vsCode);

        SDL_GPUShaderCreateInfo psInfo = {};
        psInfo.code_size = psSize;
        psInfo.code = (const Uint8*)psCode;
        psInfo.entrypoint = "PSMain";
        psInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        psInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        psInfo.num_samplers = 2;
        psInfo.num_uniform_buffers = 1;
        SDL_GPUShader* psShader = SDL_CreateGPUShader(Device, &psInfo);
        SDL_free(psCode);

        if (!vsShader || !psShader) {
            std::cerr << "Failed to create GPU Shaders: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_shader = vsShader;
        pipelineInfo.fragment_shader = psShader;

        SDL_GPUVertexInputState vertexInputState = {};
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(Vertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute vertexAttrs[4] = {};
        vertexAttrs[0].location = 0; vertexAttrs[0].buffer_slot = 0;
        vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttrs[0].offset = offsetof(Vertex, x);
        vertexAttrs[1].location = 1; vertexAttrs[1].buffer_slot = 0;
        vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttrs[1].offset = offsetof(Vertex, r);
        vertexAttrs[2].location = 2; vertexAttrs[2].buffer_slot = 0;
        vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttrs[2].offset = offsetof(Vertex, nx);
        vertexAttrs[3].location = 3; vertexAttrs[3].buffer_slot = 0;
        vertexAttrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttrs[3].offset = offsetof(Vertex, u);

        vertexInputState.num_vertex_buffers = 1;
        vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
        vertexInputState.num_vertex_attributes = 4;
        vertexInputState.vertex_attributes = vertexAttrs;
        pipelineInfo.vertex_input_state = vertexInputState;

        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.enable_stencil_test = false;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        pipelineInfo.depth_stencil_state.back_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        pipelineInfo.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        pipelineInfo.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pipelineInfo.rasterizer_state.enable_depth_bias = false;
        pipelineInfo.rasterizer_state.enable_depth_clip = false;

        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(Device, window);
        colorTargetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTargetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTargetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTargetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTargetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        colorTargetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

        Pipeline = SDL_CreateGPUGraphicsPipeline(Device, &pipelineInfo);
        SDL_ReleaseGPUShader(Device, vsShader);
        SDL_ReleaseGPUShader(Device, psShader);

        if (!Pipeline) {
            std::cerr << "Failed to create GPU Graphics Pipeline: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create shadow pipeline (depth-only, no color targets, front-face cull, depth bias)
        {
            size_t shadowVSSize;
            std::string shadowVSPath = ResolveAssetPath("engine/shaders/shadow.vs.dxil");
            void* shadowVSCode = SDL_LoadFile(shadowVSPath.c_str(), &shadowVSSize);
            if (!shadowVSCode) {
                std::cerr << "Failed to load shadow vertex shader: " << shadowVSPath << std::endl;
                return false;
            }

            SDL_GPUShaderCreateInfo shadowVSInfo = {};
            shadowVSInfo.code_size = shadowVSSize;
            shadowVSInfo.code = (const Uint8*)shadowVSCode;
            shadowVSInfo.entrypoint = "VSShadow";
            shadowVSInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
            shadowVSInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            shadowVSInfo.num_uniform_buffers = 1;  // Only UBO (MVP, Model)
            SDL_GPUShader* shadowVShader = SDL_CreateGPUShader(Device, &shadowVSInfo);
            SDL_free(shadowVSCode);

            if (!shadowVShader) {
                std::cerr << "Failed to create shadow vertex shader: " << SDL_GetError() << std::endl;
                return false;
            }

            // No-op fragment shader (SDL3 requires one even for depth-only)
            size_t shadowPSSize;
            std::string shadowPSPath = ResolveAssetPath("engine/shaders/shadow.ps.dxil");
            void* shadowPSCode = SDL_LoadFile(shadowPSPath.c_str(), &shadowPSSize);
            if (!shadowPSCode) {
                std::cerr << "Failed to load shadow fragment shader: " << shadowPSPath << std::endl;
                SDL_ReleaseGPUShader(Device, shadowVShader);
                return false;
            }

            SDL_GPUShaderCreateInfo shadowPSInfo = {};
            shadowPSInfo.code_size = shadowPSSize;
            shadowPSInfo.code = (const Uint8*)shadowPSCode;
            shadowPSInfo.entrypoint = "PSShadow";
            shadowPSInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
            shadowPSInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            shadowPSInfo.num_samplers = 2;
            shadowPSInfo.num_uniform_buffers = 0;
            SDL_GPUShader* shadowPShader = SDL_CreateGPUShader(Device, &shadowPSInfo);
            SDL_free(shadowPSCode);

            if (!shadowPShader) {
                std::cerr << "Failed to create shadow fragment shader: " << SDL_GetError() << std::endl;
                SDL_ReleaseGPUShader(Device, shadowVShader);
                return false;
            }

            SDL_GPUGraphicsPipelineCreateInfo shadowPipeInfo = {};
            shadowPipeInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            shadowPipeInfo.vertex_shader = shadowVShader;
            shadowPipeInfo.fragment_shader = shadowPShader;

            // Same vertex input layout as main pipeline
            SDL_GPUVertexInputState shadowVertInput = {};
            SDL_GPUVertexBufferDescription shadowVBDesc = {};
            shadowVBDesc.slot = 0;
            shadowVBDesc.pitch = sizeof(Vertex);
            shadowVBDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            SDL_GPUVertexAttribute shadowAttrs[4] = {};
            shadowAttrs[0].location = 0; shadowAttrs[0].buffer_slot = 0;
            shadowAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            shadowAttrs[0].offset = offsetof(Vertex, x);
            shadowAttrs[1].location = 1; shadowAttrs[1].buffer_slot = 0;
            shadowAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            shadowAttrs[1].offset = offsetof(Vertex, r);
            shadowAttrs[2].location = 2; shadowAttrs[2].buffer_slot = 0;
            shadowAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            shadowAttrs[2].offset = offsetof(Vertex, nx);
            shadowAttrs[3].location = 3; shadowAttrs[3].buffer_slot = 0;
            shadowAttrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            shadowAttrs[3].offset = offsetof(Vertex, u);
            shadowVertInput.num_vertex_buffers = 1;
            shadowVertInput.vertex_buffer_descriptions = &shadowVBDesc;
            shadowVertInput.num_vertex_attributes = 4;
            shadowVertInput.vertex_attributes = shadowAttrs;
            shadowPipeInfo.vertex_input_state = shadowVertInput;

            // Depth test enabled, no color targets
            shadowPipeInfo.depth_stencil_state.enable_depth_test = true;
            shadowPipeInfo.depth_stencil_state.enable_depth_write = true;
            shadowPipeInfo.depth_stencil_state.enable_stencil_test = false;
            shadowPipeInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
            shadowPipeInfo.depth_stencil_state.back_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
            shadowPipeInfo.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
            shadowPipeInfo.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

            // No culling - render all faces to avoid hollow shadows
            shadowPipeInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            shadowPipeInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
            shadowPipeInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            shadowPipeInfo.rasterizer_state.enable_depth_bias = false;
            shadowPipeInfo.rasterizer_state.enable_depth_clip = false;

            // No color targets, depth-only target
            shadowPipeInfo.target_info.num_color_targets = 0;
            shadowPipeInfo.target_info.color_target_descriptions = nullptr;
            shadowPipeInfo.target_info.has_depth_stencil_target = true;
            shadowPipeInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

            ShadowPipeline = SDL_CreateGPUGraphicsPipeline(Device, &shadowPipeInfo);
            SDL_ReleaseGPUShader(Device, shadowVShader);
            SDL_ReleaseGPUShader(Device, shadowPShader);

            if (!ShadowPipeline) {
                std::cerr << "Failed to create shadow pipeline: " << SDL_GetError() << std::endl;
                return false;
            }
        }

        Vertex vertices[] = {
            { -0.5f, -0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1, 0, 0 },
            {  0.5f, -0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1, 0, 0 },
            {  0.5f,  0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1, 0, 0 },
            { -0.5f,  0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1, 0, 0 },
            { -0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1, 0, 0 },
            { -0.5f,  0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1, 0, 0 },
            {  0.5f,  0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1, 0, 0 },
            {  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1, 0, 0 },
            { -0.5f, -0.5f, -0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0, 0, 0 },
            { -0.5f, -0.5f,  0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0, 0, 0 },
            { -0.5f,  0.5f,  0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0, 0, 0 },
            { -0.5f,  0.5f, -0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0, 0, 0 },
            {  0.5f, -0.5f,  0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0, 0, 0 },
            {  0.5f, -0.5f, -0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0, 0, 0 },
            {  0.5f,  0.5f, -0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0, 0, 0 },
            {  0.5f,  0.5f,  0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0, 0, 0 },
            { -0.5f,  0.5f,  0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0, 0, 0 },
            {  0.5f,  0.5f,  0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0, 0, 0 },
            {  0.5f,  0.5f, -0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0, 0, 0 },
            { -0.5f,  0.5f, -0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0, 0, 0 },
            { -0.5f, -0.5f, -0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0, 0, 0 },
            {  0.5f, -0.5f, -0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0, 0, 0 },
            {  0.5f, -0.5f,  0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0, 0, 0 },
            { -0.5f, -0.5f,  0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0, 0, 0 }
        };

        uint32_t indices[] = {
            0, 3, 2,  2, 1, 0,
            4, 7, 6,  6, 5, 4,
            9, 8, 11,  11, 10, 9,
            13, 12, 15,  15, 14, 13,
            19, 18, 17,  17, 16, 19,
            23, 22, 21,  21, 20, 23
        };

        VertexBuffer = CreateAndUploadBuffer(vertices, sizeof(vertices), SDL_GPU_BUFFERUSAGE_VERTEX);
        IndexBuffer = CreateAndUploadBuffer(indices, sizeof(indices), SDL_GPU_BUFFERUSAGE_INDEX);

        {
            const float gridSize = 15.0f;
            const int cells = 15;
            const float cellSize = gridSize * 2.0f / cells;
            std::vector<Vertex> gridVerts;
            std::vector<uint32_t> gridIdx;

            for (int cx = 0; cx < cells; cx++) {
                for (int cz = 0; cz < cells; cz++) {
                    float x0 = -gridSize + cx * cellSize;
                    float z0 = -gridSize + cz * cellSize;
                    float x1 = x0 + cellSize;
                    float z1 = z0 + cellSize;

                    bool light = ((cx + cz) % 2) == 0;
                    float c = light ? 0.35f : 0.25f;
                    int base = (int)gridVerts.size();

                    gridVerts.push_back({ x0, -0.005f, z0, c, c, c, 1.0f, 0, 1, 0, 0, 0 });
                    gridVerts.push_back({ x1, -0.005f, z0, c, c, c, 1.0f, 0, 1, 0, 0, 0 });
                    gridVerts.push_back({ x1, -0.005f, z1, c, c, c, 1.0f, 0, 1, 0, 0, 0 });
                    gridVerts.push_back({ x0, -0.005f, z1, c, c, c, 1.0f, 0, 1, 0, 0, 0 });

                    gridIdx.push_back(base);
                    gridIdx.push_back(base + 1);
                    gridIdx.push_back(base + 2);
                    gridIdx.push_back(base);
                    gridIdx.push_back(base + 2);
                    gridIdx.push_back(base + 3);
                }
            }

            GridIndexCount = (int)gridIdx.size();
            GridBuffer = CreateAndUploadBuffer(gridVerts.data(), (Uint32)(gridVerts.size() * sizeof(Vertex)), SDL_GPU_BUFFERUSAGE_VERTEX);
            GridIndexBuffer = CreateAndUploadBuffer(gridIdx.data(), (Uint32)(gridIdx.size() * sizeof(uint32_t)), SDL_GPU_BUFFERUSAGE_INDEX);
        }

        // Build gizmo primitive meshes
        auto buildShape = [&](const MeshBuilder::Mesh& mesh, SDL_GPUBuffer*& vb, SDL_GPUBuffer*& ib, uint32_t& idxCount) {
            idxCount = (uint32_t)mesh.indices.size();
            vb = CreateAndUploadBuffer(mesh.vertices.data(), (Uint32)(mesh.vertices.size() * sizeof(Vertex)), SDL_GPU_BUFFERUSAGE_VERTEX);
            ib = CreateAndUploadBuffer(mesh.indices.data(), (Uint32)(mesh.indices.size() * sizeof(uint32_t)), SDL_GPU_BUFFERUSAGE_INDEX);
        };

        buildShape(MeshBuilder::Cone(1.0f, 1.0f, 16),           ConeVB, ConeIB, ConeIndexCount);
        buildShape(MeshBuilder::Cylinder(1.0f, 1.0f, 16),        CylinderVB, CylinderIB, CylinderIndexCount);
        buildShape(MeshBuilder::Sphere(1.0f, 12, 16),            SphereVB, SphereIB, SphereIndexCount);
        buildShape(MeshBuilder::Wedge(1.0f, 1.0f, 1.0f),         WedgeVB, WedgeIB, WedgeIndexCount);

        // Create shadow map texture (2048x2048, depth-only, sampleable)
        {
            SDL_GPUTextureCreateInfo shadowInfo = {};
            shadowInfo.type = SDL_GPU_TEXTURETYPE_2D;
            shadowInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
            shadowInfo.width = 2048;
            shadowInfo.height = 2048;
            shadowInfo.layer_count_or_depth = 1;
            shadowInfo.num_levels = 1;
            shadowInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            ShadowMap = SDL_CreateGPUTexture(Device, &shadowInfo);
            if (!ShadowMap) {
                std::cerr << "Failed to create shadow map: " << SDL_GetError() << std::endl;
            }
        }

        // Create comparison sampler for PCF shadow filtering
        {
            SDL_GPUSamplerCreateInfo sampInfo = {};
            sampInfo.min_filter = SDL_GPU_FILTER_LINEAR;
            sampInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
            sampInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            sampInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            sampInfo.compare_op = SDL_GPU_COMPAREOP_LESS;
            sampInfo.enable_compare = true;
            ShadowSampler = SDL_CreateGPUSampler(Device, &sampInfo);
            if (!ShadowSampler) {
                std::cerr << "Failed to create shadow sampler: " << SDL_GetError() << std::endl;
            }
        }

        // Create skybox texture, sampler, and pipeline
        {
            // Load sky texture using SDL3_image
            std::string skyPath = ResolveAssetPath("engine/shaders/base_sky.jpeg");
            SDL_GPUCommandBuffer* skyCmd = SDL_AcquireGPUCommandBuffer(Device);
            SDL_GPUCopyPass* skyCopy = SDL_BeginGPUCopyPass(skyCmd);
            int skyW = 0, skyH = 0;
            SkyTexture = IMG_LoadGPUTexture(Device, skyCopy, skyPath.c_str(), &skyW, &skyH);
            SDL_EndGPUCopyPass(skyCopy);
            SDL_SubmitGPUCommandBuffer(skyCmd);
            if (!SkyTexture) {
                std::cerr << "Failed to load sky texture: " << SDL_GetError() << std::endl;
            } else {
                std::cout << "Sky texture loaded: " << skyW << "x" << skyH << std::endl;
            }

            // Sky sampler (linear, repeat wrap)
            SDL_GPUSamplerCreateInfo skySampInfo = {};
            skySampInfo.min_filter = SDL_GPU_FILTER_LINEAR;
            skySampInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
            skySampInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
            skySampInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            skySampInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            skySampInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            skySampInfo.enable_compare = false;
            SkySampler = SDL_CreateGPUSampler(Device, &skySampInfo);
            if (!SkySampler) {
                std::cerr << "Failed to create sky sampler: " << SDL_GetError() << std::endl;
            }

            // Sky pipeline (front-face culling = see inside, no depth write)
            size_t skyVSSize, skyPSSize;
            std::string skyVSPath = ResolveAssetPath("engine/shaders/sky.vs.dxil");
            std::string skyPSPath = ResolveAssetPath("engine/shaders/sky.ps.dxil");
            void* skyVSCode = SDL_LoadFile(skyVSPath.c_str(), &skyVSSize);
            void* skyPSCode = SDL_LoadFile(skyPSPath.c_str(), &skyPSSize);

            if (!skyVSCode || !skyPSCode) {
                std::cerr << "Failed to load sky shaders" << std::endl;
                return false;
            }

            SDL_GPUShaderCreateInfo skyVSInfo = {};
            skyVSInfo.code_size = skyVSSize;
            skyVSInfo.code = (const Uint8*)skyVSCode;
            skyVSInfo.entrypoint = "VSSky";
            skyVSInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
            skyVSInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            skyVSInfo.num_uniform_buffers = 1;
            SDL_GPUShader* skyVShader = SDL_CreateGPUShader(Device, &skyVSInfo);
            SDL_free(skyVSCode);

            SDL_GPUShaderCreateInfo skyPSInfo = {};
            skyPSInfo.code_size = skyPSSize;
            skyPSInfo.code = (const Uint8*)skyPSCode;
            skyPSInfo.entrypoint = "PSSky";
            skyPSInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
            skyPSInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            skyPSInfo.num_samplers = 2;  // shadow sampler (s0) + sky sampler (s1)
            skyPSInfo.num_uniform_buffers = 1;
            SDL_GPUShader* skyPShader = SDL_CreateGPUShader(Device, &skyPSInfo);
            SDL_free(skyPSCode);

            if (!skyVShader || !skyPShader) {
                std::cerr << "Failed to create sky shaders: " << SDL_GetError() << std::endl;
                return false;
            }

            SDL_GPUGraphicsPipelineCreateInfo skyPipeInfo = {};
            skyPipeInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
            skyPipeInfo.vertex_shader = skyVShader;
            skyPipeInfo.fragment_shader = skyPShader;

            // Same vertex input layout
            SDL_GPUVertexInputState skyVertInput = {};
            SDL_GPUVertexBufferDescription skyVBDesc = {};
            skyVBDesc.slot = 0;
            skyVBDesc.pitch = sizeof(Vertex);
            skyVBDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
            SDL_GPUVertexAttribute skyAttrs[4] = {};
            skyAttrs[0].location = 0; skyAttrs[0].buffer_slot = 0;
            skyAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            skyAttrs[0].offset = offsetof(Vertex, x);
            skyAttrs[1].location = 1; skyAttrs[1].buffer_slot = 0;
            skyAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            skyAttrs[1].offset = offsetof(Vertex, r);
            skyAttrs[2].location = 2; skyAttrs[2].buffer_slot = 0;
            skyAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
            skyAttrs[2].offset = offsetof(Vertex, nx);
            skyAttrs[3].location = 3; skyAttrs[3].buffer_slot = 0;
            skyAttrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
            skyAttrs[3].offset = offsetof(Vertex, u);
            skyVertInput.num_vertex_buffers = 1;
            skyVertInput.vertex_buffer_descriptions = &skyVBDesc;
            skyVertInput.num_vertex_attributes = 4;
            skyVertInput.vertex_attributes = skyAttrs;
            skyPipeInfo.vertex_input_state = skyVertInput;

            // Depth test enabled but NO depth write (skybox is always behind scene)
            skyPipeInfo.depth_stencil_state.enable_depth_test = true;
            skyPipeInfo.depth_stencil_state.enable_depth_write = false;
            skyPipeInfo.depth_stencil_state.enable_stencil_test = false;
            skyPipeInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
            skyPipeInfo.depth_stencil_state.back_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.back_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.back_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.back_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
            skyPipeInfo.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
            skyPipeInfo.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;

            // Front-face culling (so we see the inside of the box)
            skyPipeInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
            skyPipeInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
            skyPipeInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            skyPipeInfo.rasterizer_state.enable_depth_bias = false;
            skyPipeInfo.rasterizer_state.enable_depth_clip = false;

            // Same color target format as main pipeline
            SDL_GPUColorTargetDescription skyColorDesc = {};
            skyColorDesc.format = SDL_GetGPUSwapchainTextureFormat(Device, window);
            skyColorDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            skyColorDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            skyColorDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
            skyColorDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            skyColorDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            skyColorDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

            skyPipeInfo.target_info.num_color_targets = 1;
            skyPipeInfo.target_info.color_target_descriptions = &skyColorDesc;
            skyPipeInfo.target_info.has_depth_stencil_target = true;
            skyPipeInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

            SkyPipeline = SDL_CreateGPUGraphicsPipeline(Device, &skyPipeInfo);
            SDL_ReleaseGPUShader(Device, skyVShader);
            SDL_ReleaseGPUShader(Device, skyPShader);

            if (!SkyPipeline) {
                std::cerr << "Failed to create sky pipeline: " << SDL_GetError() << std::endl;
            }
        }

        // Create default 1x1 white texture for untextured objects
        {
            SDL_GPUTextureCreateInfo texInfo = {};
            texInfo.type = SDL_GPU_TEXTURETYPE_2D;
            texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            texInfo.width = 1;
            texInfo.height = 1;
            texInfo.layer_count_or_depth = 1;
            texInfo.num_levels = 1;
            texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
            DefaultTexture = SDL_CreateGPUTexture(Device, &texInfo);
            if (!DefaultTexture) {
                std::cerr << "Failed to create default texture: " << SDL_GetError() << std::endl;
            }

            // Upload white pixel
            uint32_t whitePixel = 0xFFFFFFFF;
            SDL_GPUTransferBufferCreateInfo tbInfo = {};
            tbInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            tbInfo.size = sizeof(uint32_t);
            SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(Device, &tbInfo);
            if (tb) {
                void* map = SDL_MapGPUTransferBuffer(Device, tb, false);
                std::memcpy(map, &whitePixel, sizeof(uint32_t));
                SDL_UnmapGPUTransferBuffer(Device, tb);

                SDL_GPUCommandBuffer* defCmd = SDL_AcquireGPUCommandBuffer(Device);
                SDL_GPUCopyPass* defCopy = SDL_BeginGPUCopyPass(defCmd);
                SDL_GPUTextureTransferInfo srcInfo = {};
                srcInfo.transfer_buffer = tb;
                srcInfo.offset = 0;
                SDL_GPUTextureRegion dstRegion = {};
                dstRegion.texture = DefaultTexture;
                dstRegion.w = 1; dstRegion.h = 1; dstRegion.d = 1;
                SDL_UploadToGPUTexture(defCopy, &srcInfo, &dstRegion, false);
                SDL_EndGPUCopyPass(defCopy);
                SDL_SubmitGPUCommandBuffer(defCmd);
                SDL_ReleaseGPUTransferBuffer(Device, tb);
            }
        }

        // Create default sampler (linear, wrap)
        {
            SDL_GPUSamplerCreateInfo sampInfo = {};
            sampInfo.min_filter = SDL_GPU_FILTER_LINEAR;
            sampInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
            sampInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            sampInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            sampInfo.enable_compare = false;
            DefaultSampler = SDL_CreateGPUSampler(Device, &sampInfo);
            if (!DefaultSampler) {
                std::cerr << "Failed to create default sampler: " << SDL_GetError() << std::endl;
            }
        }

        return (VertexBuffer && IndexBuffer);
    }
};
