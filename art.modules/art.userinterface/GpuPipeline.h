#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <filesystem>

inline std::string ResolveAssetPath(const std::string& relativePath) {
    // 1. Check current directory relative path
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }

    // 2. Check parent directory relative path
    std::string parentPath = "../" + relativePath;
    if (std::filesystem::exists(parentPath)) {
        return parentPath;
    }

    // 3. Check grandparent directory relative path
    std::string grandparentPath = "../../" + relativePath;
    if (std::filesystem::exists(grandparentPath)) {
        return grandparentPath;
    }

    // 4. Try using SDL_GetBasePath
    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        std::string base(basePath);

        std::string resolved = base + relativePath;
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }

        resolved = base + "../" + relativePath;
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }

        resolved = base + "../../" + relativePath;
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }
    }

    // Default fallback
    return relativePath;
}

struct Vertex
{
    float x, y, z;       // Position
    float r, g, b, a;   // Color
    float nx, ny, nz;   // Normal
};

class GpuPipeline
{
public:
    SDL_GPUDevice* Device = nullptr;
    SDL_GPUGraphicsPipeline* Pipeline = nullptr;
    SDL_GPUBuffer* VertexBuffer = nullptr;
    SDL_GPUBuffer* IndexBuffer = nullptr;

    GpuPipeline() {}

    ~GpuPipeline() {
        Cleanup();
    }

    void Cleanup() {
        if (VertexBuffer) {
            SDL_ReleaseGPUBuffer(Device, VertexBuffer);
            VertexBuffer = nullptr;
        }
        if (IndexBuffer) {
            SDL_ReleaseGPUBuffer(Device, IndexBuffer);
            IndexBuffer = nullptr;
        }
        if (Pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(Device, Pipeline);
            Pipeline = nullptr;
        }
        if (Device) {
            SDL_DestroyGPUDevice(Device);
            Device = nullptr;
        }
    }

    SDL_GPUBuffer* CreateAndUploadBuffer(const void* data, Uint32 size, SDL_GPUBufferUsageFlags usage) {
        // Create destination buffer
        SDL_GPUBufferCreateInfo bufferInfo = {};
        bufferInfo.usage = usage;
        bufferInfo.size = size;
        SDL_GPUBuffer* destBuffer = SDL_CreateGPUBuffer(Device, &bufferInfo);
        if (!destBuffer) {
            std::cerr << "Failed to create GPU Buffer: " << SDL_GetError() << std::endl;
            return nullptr;
        }

        // Create staging (transfer) buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = size;
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(Device, &transferInfo);
        if (!transferBuffer) {
            std::cerr << "Failed to create transfer buffer: " << SDL_GetError() << std::endl;
            SDL_ReleaseGPUBuffer(Device, destBuffer);
            return nullptr;
        }

        // Map and copy data
        void* map = SDL_MapGPUTransferBuffer(Device, transferBuffer, false);
        std::memcpy(map, data, size);
        SDL_UnmapGPUTransferBuffer(Device, transferBuffer);

        // Record copy command
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

        // Submit and release
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(Device, transferBuffer);

        return destBuffer;
    }

    bool Initialize(SDL_Window* window) {
        // Create GPU Device (allowing DXIL and DXBC formats)
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

        // Claim window for the swapchain
        if (!SDL_ClaimWindowForGPUDevice(Device, window)) {
            std::cerr << "Failed to claim window: " << SDL_GetError() << std::endl;
            return false;
        }

        SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(Device, window);
        std::cout << "Swapchain Texture Format: " << (int)swapchainFormat << std::endl;

        // Load Shaders
        size_t vsSize, psSize;
        std::string vsPath = ResolveAssetPath("shaders/shader.vs.dxil");
        std::string psPath = ResolveAssetPath("shaders/shader.ps.dxil");
        void* vsCode = SDL_LoadFile(vsPath.c_str(), &vsSize);
        void* psCode = SDL_LoadFile(psPath.c_str(), &psSize);

        if (!vsCode || !psCode) {
            std::cerr << "Failed to load shader binaries from resolved paths: " << vsPath << " or " << psPath << std::endl;
            return false;
        }

        // Create Vertex Shader
        SDL_GPUShaderCreateInfo vsInfo = {};
        vsInfo.code_size = vsSize;
        vsInfo.code = (const Uint8*)vsCode;
        vsInfo.entrypoint = "VSMain";
        vsInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        vsInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vsInfo.num_uniform_buffers = 1; // 1 uniform buffer at slot 0 (MVP matrix)

        SDL_GPUShader* vsShader = SDL_CreateGPUShader(Device, &vsInfo);
        SDL_free(vsCode);

        // Create Pixel Shader
        SDL_GPUShaderCreateInfo psInfo = {};
        psInfo.code_size = psSize;
        psInfo.code = (const Uint8*)psCode;
        psInfo.entrypoint = "PSMain";
        psInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        psInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        psInfo.num_uniform_buffers = 1; // Light UBO at slot 0

        SDL_GPUShader* psShader = SDL_CreateGPUShader(Device, &psInfo);
        SDL_free(psCode);

        if (!vsShader || !psShader) {
            std::cerr << "Failed to create GPU Shaders: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create Graphics Pipeline
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.vertex_shader = vsShader;
        pipelineInfo.fragment_shader = psShader;

        // Vertex Input Layout
        SDL_GPUVertexInputState vertexInputState = {};
        
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(Vertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        
        SDL_GPUVertexAttribute vertexAttrs[3] = {};
        // Position
        vertexAttrs[0].location = 0;
        vertexAttrs[0].buffer_slot = 0;
        vertexAttrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttrs[0].offset = offsetof(Vertex, x);
        // Color
        vertexAttrs[1].location = 1;
        vertexAttrs[1].buffer_slot = 0;
        vertexAttrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttrs[1].offset = offsetof(Vertex, r);
        // Normal
        vertexAttrs[2].location = 2;
        vertexAttrs[2].buffer_slot = 0;
        vertexAttrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttrs[2].offset = offsetof(Vertex, nx);

        vertexInputState.num_vertex_buffers = 1;
        vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
        vertexInputState.num_vertex_attributes = 3;
        vertexInputState.vertex_attributes = vertexAttrs;

        pipelineInfo.vertex_input_state = vertexInputState;

        // Depth Stencil State (D3D12 requires valid enum values even if disabled)
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

        // Rasterizer State
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pipelineInfo.rasterizer_state.enable_depth_bias = false;
        pipelineInfo.rasterizer_state.enable_depth_clip = false;

        // Render Targets (Match the swapchain format)
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(Device, window);
        
        // Blend State (D3D12 requires valid enum values even if disabled)
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
        
        // Shaders compiled into pipeline, release raw shaders
        SDL_ReleaseGPUShader(Device, vsShader);
        SDL_ReleaseGPUShader(Device, psShader);

        if (!Pipeline) {
            std::cerr << "Failed to create GPU Graphics Pipeline: " << SDL_GetError() << std::endl;
            return false;
        }

        // 24 vertices, 4 per face, with flat normals (pos x,y,z | color r,g,b,a | normal nx,ny,nz)
        Vertex vertices[] = {
            // Front (Red)   normal +Z
            { -0.5f, -0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1 },
            {  0.5f, -0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1 },
            {  0.5f,  0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1 },
            { -0.5f,  0.5f,  0.5f,  0.85f, 0.1f, 0.1f, 1.0f,   0, 0, 1 },
            // Back (Orange)  normal -Z
            { -0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1 },
            { -0.5f,  0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1 },
            {  0.5f,  0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1 },
            {  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f,   0, 0,-1 },
            // Left (Green)   normal -X
            { -0.5f, -0.5f, -0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0 },
            { -0.5f, -0.5f,  0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0 },
            { -0.5f,  0.5f,  0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0 },
            { -0.5f,  0.5f, -0.5f,  0.1f, 0.7f, 0.1f, 1.0f,  -1, 0, 0 },
            // Right (Blue)   normal +X
            {  0.5f, -0.5f,  0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0 },
            {  0.5f, -0.5f, -0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0 },
            {  0.5f,  0.5f, -0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0 },
            {  0.5f,  0.5f,  0.5f,  0.1f, 0.35f, 0.9f, 1.0f,  1, 0, 0 },
            // Top (White)    normal +Y
            { -0.5f,  0.5f,  0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0 },
            {  0.5f,  0.5f,  0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0 },
            {  0.5f,  0.5f, -0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0 },
            { -0.5f,  0.5f, -0.5f,  0.95f, 0.95f, 0.95f, 1.0f,  0, 1, 0 },
            // Bottom (Yellow) normal -Y
            { -0.5f, -0.5f, -0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0 },
            {  0.5f, -0.5f, -0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0 },
            {  0.5f, -0.5f,  0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0 },
            { -0.5f, -0.5f,  0.5f,  0.9f, 0.85f, 0.1f, 1.0f,  0,-1, 0 }
        };

        uint32_t indices[] = {
            0, 3, 2,  2, 1, 0,       // Front (Red)
            4, 7, 6,  6, 5, 4,       // Back (Orange)
            9, 8, 11,  11, 10, 9,    // Left (Green)
            13, 12, 15,  15, 14, 13, // Right (Blue)
            19, 18, 17,  17, 16, 19, // Top (White)
            23, 22, 21,  21, 20, 23  // Bottom (Yellow)
        };

        VertexBuffer = CreateAndUploadBuffer(vertices, sizeof(vertices), SDL_GPU_BUFFERUSAGE_VERTEX);
        IndexBuffer = CreateAndUploadBuffer(indices, sizeof(indices), SDL_GPU_BUFFERUSAGE_INDEX);

        return (VertexBuffer && IndexBuffer);
    }
};
