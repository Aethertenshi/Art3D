#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include "core/Matrix4x4.h"
#include "core/GameObject.h"
#include "render/GpuPipeline.h"
#include "render/GpuTypes.h"

class LightSource;

class Renderer {
public:
    enum GizmoTool { ToolDrag, ToolMove, ToolRotate, ToolScale };

    Renderer(GpuPipeline& pipeline) : m_Pipeline(pipeline) {}

    LightUBO CollectLights(const std::vector<std::shared_ptr<GameObject>>& sceneObjects);

    void RenderGrid(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                    const Matrix4x4& view, const Matrix4x4& proj,
                    const LightUBO& lightData);

    void Render3DObjects(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                         const std::vector<std::shared_ptr<GameObject>>& sceneObjects,
                         const Matrix4x4& view, const Matrix4x4& proj,
                         const LightUBO& lightData);

    void Render2DObjects(const std::vector<std::shared_ptr<GameObject>>& sceneObjects);

    void RenderGizmo(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                     GameObject* selectedObject, GizmoTool tool,
                     const Matrix4x4& view, const Matrix4x4& proj,
                     const LightUBO& lightData);

    // Shadow pass: renders scene from light's POV into shadow map.
    // Returns the light ViewProj matrix (identity if no shadow cast).
    Matrix4x4 RenderShadowPass(SDL_GPUCommandBuffer* cmd,
                               LightSource* shadowLight,
                               const std::vector<std::shared_ptr<GameObject>>& sceneObjects);

    // Render visual sun (glowing sphere) in the sky
    void RenderSun(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                   class Sun* sun, const Matrix4x4& view, const Matrix4x4& proj);

    // Render skybox (large inverted cube with sky texture)
    void RenderSky(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                   float camX, float camY, float camZ,
                   const Matrix4x4& view, const Matrix4x4& proj);

    static void Render3DObject(GameObject* obj, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                               const Matrix4x4& view, const Matrix4x4& proj,
                               GpuPipeline& pipeline, const LightUBO& lightData);

    static void Render2DObject(GameObject* obj);

private:
    GpuPipeline& m_Pipeline;
};
