#include "Renderer.h"
#include "scene/LightSource.h"
#include "scene/Sun.h"
#include "ui/BaseDrawables.h"
#include "ui/Frame.h"
#include "imgui.h"
#include <functional>

LightUBO Renderer::CollectLights(const std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    LightUBO lightData = {};
    for (auto& obj : sceneObjects) {
        if ((obj->ClassName == "LightSource" || obj->ClassName == "Sun") && lightData.lightCount < 8) {
            LightSource* light = static_cast<LightSource*>(obj.get());
            Vector3D worldPos = light->GetWorldPosition();
            auto& gpuLight = lightData.lights[lightData.lightCount];
            gpuLight.pos[0] = worldPos.X;
            gpuLight.pos[1] = worldPos.Y;
            gpuLight.pos[2] = worldPos.Z;
            gpuLight.pos[3] = light->Intensity;
            gpuLight.dir[0] = light->Direction.X;
            gpuLight.dir[1] = light->Direction.Y;
            gpuLight.dir[2] = light->Direction.Z;
            gpuLight.dir[3] = light->Range;
            gpuLight.color[0] = light->Color.X;
            gpuLight.color[1] = light->Color.Y;
            gpuLight.color[2] = light->Color.Z;
            gpuLight.color[3] = (float)light->Type;
            float innerRad = cosf(light->SpotAngleInner * 3.14159265f / 180.0f);
            float outerRad = cosf(light->SpotAngleOuter * 3.14159265f / 180.0f);
            gpuLight.spotAngles[0] = innerRad;
            gpuLight.spotAngles[1] = outerRad;
            lightData.lightCount++;
        }
    }
    lightData.ambientIntensity = 0.35f;
    return lightData;
}

void Renderer::RenderGrid(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                          const Matrix4x4& view, const Matrix4x4& proj,
                          const LightUBO& lightData) {
    SDL_GPUBufferBinding gridVB = {};
    gridVB.buffer = m_Pipeline.GridBuffer;
    gridVB.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &gridVB, 1);

    SDL_GPUBufferBinding gridIB = {};
    gridIB.buffer = m_Pipeline.GridIndexBuffer;
    gridIB.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &gridIB, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    Matrix4x4 gridModel = Matrix4x4::Identity();
    Matrix4x4 gridMVP = gridModel.Multiply(view).Multiply(proj);

    struct GridUniforms {
        Matrix4x4 MVP;
        Matrix4x4 Model;
        float ColorOverride[4];
    } gridUniform;
    gridUniform.MVP = gridMVP;
    gridUniform.Model = gridModel;
    gridUniform.ColorOverride[0] = 0.0f;
    gridUniform.ColorOverride[1] = 0.0f;
    gridUniform.ColorOverride[2] = 0.0f;
    gridUniform.ColorOverride[3] = 0.0f;

    SDL_PushGPUVertexUniformData(cmd, 0, &gridUniform, sizeof(gridUniform));
    SDL_DrawGPUIndexedPrimitives(renderPass, m_Pipeline.GridIndexCount, 1, 0, 0, 0);
}

void Renderer::Render3DObjects(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                               const std::vector<std::shared_ptr<GameObject>>& sceneObjects,
                               const Matrix4x4& view, const Matrix4x4& proj,
                               const LightUBO& lightData) {
    SDL_GPUBufferBinding vb = {};
    vb.buffer = m_Pipeline.VertexBuffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vb, 1);

    SDL_GPUBufferBinding ib = {};
    ib.buffer = m_Pipeline.IndexBuffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    for (auto& obj : sceneObjects) {
        Render3DObject(obj.get(), renderPass, cmd, view, proj, m_Pipeline, lightData);
    }
}

void Renderer::Render3DObject(GameObject* obj, SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                              const Matrix4x4& view, const Matrix4x4& proj,
                              GpuPipeline& pipeline, const LightUBO& lightData) {
    if (!obj) return;
    if (obj->ClassName == "Sun") { // Sun is rendered separately as sky sphere, skip here
        for (auto& child : obj->Children) {
            Render3DObject(child.get(), renderPass, cmd, view, proj, pipeline, lightData);
        }
        return;
    }
    if (obj->Is3D) {
        Vector3D worldPos = obj->GetWorldPosition();
        Vector3D worldRot = obj->GetWorldRotation();
        Vector3D worldScale = obj->GetWorldScale();

        float rotX = worldRot.X * 3.14159265f / 180.0f;
        float rotY = worldRot.Y * 3.14159265f / 180.0f;
        float rotZ = worldRot.Z * 3.14159265f / 180.0f;

        float scaleX = worldScale.X;
        float scaleY = worldScale.Y;
        float scaleZ = worldScale.Z;
        if (obj->ClassName == "LightSource") {
            scaleX *= 0.15f; scaleY *= 0.15f; scaleZ *= 0.15f;
        }

        float offRotX = obj->RotationOffset.X * 3.14159265f / 180.0f;
        float offRotY = obj->RotationOffset.Y * 3.14159265f / 180.0f;
        float offRotZ = obj->RotationOffset.Z * 3.14159265f / 180.0f;

        Matrix4x4 model = Matrix4x4::Translation(obj->PositionOffset.X, obj->PositionOffset.Y, obj->PositionOffset.Z)
            .Multiply(Matrix4x4::RotationZ(offRotZ))
            .Multiply(Matrix4x4::RotationX(offRotX))
            .Multiply(Matrix4x4::RotationY(offRotY))
            .Multiply(Matrix4x4::Scale(scaleX, scaleY, scaleZ))
            .Multiply(Matrix4x4::RotationZ(rotZ))
            .Multiply(Matrix4x4::RotationX(rotX))
            .Multiply(Matrix4x4::RotationY(rotY))
            .Multiply(Matrix4x4::Translation(worldPos.X, worldPos.Y, worldPos.Z));

        Matrix4x4 mvp = model.Multiply(view).Multiply(proj);

        struct VertexUniforms {
            Matrix4x4 MVP;
            Matrix4x4 Model;
            float ColorOverride[4];
        } vsUniform;
        vsUniform.MVP = mvp;
        vsUniform.Model = model;
        vsUniform.ColorOverride[0] = 0.0f;
        vsUniform.ColorOverride[1] = 0.0f;
        vsUniform.ColorOverride[2] = 0.0f;
        vsUniform.ColorOverride[3] = 0.0f;

        SDL_GPUBufferBinding vertexBinding = {};
        vertexBinding.buffer = obj->CustomVertexBuffer ? obj->CustomVertexBuffer.get() : pipeline.VertexBuffer;
        vertexBinding.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        SDL_GPUBufferBinding indexBinding = {};
        indexBinding.buffer = obj->CustomIndexBuffer ? obj->CustomIndexBuffer.get() : pipeline.IndexBuffer;
        indexBinding.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_PushGPUVertexUniformData(cmd, 0, &vsUniform, sizeof(vsUniform));

        if (!obj->CustomSubMeshes.empty()) {
            for (auto& sm : obj->CustomSubMeshes) {
                SDL_GPUTextureSamplerBinding texBinding = {};
                SDL_GPUTexture* diffuseTex = sm.Texture ? sm.Texture.get() : pipeline.DefaultTexture;
                texBinding.texture = diffuseTex;
                texBinding.sampler = pipeline.DefaultSampler;
                SDL_BindGPUFragmentSamplers(renderPass, 1, &texBinding, 1);

                SDL_DrawGPUIndexedPrimitives(renderPass, sm.IndexCount, 1, sm.IndexStart, 0, 0);
            }
        } else {
            SDL_GPUTextureSamplerBinding texBinding = {};
            texBinding.texture = pipeline.DefaultTexture;
            texBinding.sampler = pipeline.DefaultSampler;
            SDL_BindGPUFragmentSamplers(renderPass, 1, &texBinding, 1);

            uint32_t indexCount = obj->CustomIndexBuffer ? obj->CustomIndexCount : 36;
            SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, 1, 0, 0, 0);
        }
    }

    for (auto& child : obj->Children) {
        Render3DObject(child.get(), renderPass, cmd, view, proj, pipeline, lightData);
    }
}

void Renderer::Render2DObject(GameObject* obj) {
    if (!obj) return;
    if (!obj->Is3D) {
        if (obj->ClassName == "Frame" || obj->ClassName == "BaseDrawables") {
            BaseDrawables* drawable = static_cast<BaseDrawables*>(obj);
            drawable->Draw();
        } else if (obj->ClassName == "GameObject") {
            ImGui::GetForegroundDrawList()->AddRectFilled(
                ImVec2((float)obj->Position.OffsetX, (float)obj->Position.OffsetY),
                ImVec2((float)(obj->Position.OffsetX + obj->Size.OffsetX), (float)(obj->Position.OffsetY + obj->Size.OffsetY)),
                IM_COL32(100, 100, 100, 50)
            );
            ImGui::GetForegroundDrawList()->AddRect(
                ImVec2((float)obj->Position.OffsetX, (float)obj->Position.OffsetY),
                ImVec2((float)(obj->Position.OffsetX + obj->Size.OffsetX), (float)(obj->Position.OffsetY + obj->Size.OffsetY)),
                IM_COL32(150, 150, 150, 255)
            );
        }
    }
    for (auto& child : obj->Children) {
        Render2DObject(child.get());
    }
}

void Renderer::Render2DObjects(const std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    for (auto& obj : sceneObjects) {
        Render2DObject(obj.get());
    }
}

void Renderer::RenderGizmo(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                           GameObject* selectedObject, GizmoTool tool,
                           const Matrix4x4& view, const Matrix4x4& proj,
                           const LightUBO& lightData) {
    if (!selectedObject || tool == ToolDrag) return;

    // Unlit: push neutral light UBO (no lights, full ambient)
    LightUBO gizmoLight = {};
    gizmoLight.lightCount = 0;
    gizmoLight.ambientIntensity = 1.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &gizmoLight, sizeof(gizmoLight));

    Vector3D wp = selectedObject->GetWorldPosition();
    Vector3D ws = selectedObject->GetWorldScale();
    float hx = ws.X * 0.5f, hy = ws.Y * 0.5f, hz = ws.Z * 0.5f;

    // Build model matrix: Scale → Rotation → Translation (row-vector convention)
    auto model = [&](float sx, float sy, float sz,
                     const Matrix4x4& rot,
                     float tx, float ty, float tz) {
        return Matrix4x4::Scale(sx, sy, sz)
            .Multiply(rot)
            .Multiply(Matrix4x4::Translation(tx, ty, tz));
    };

    auto pushDraw = [&](const Matrix4x4& mdl, float r, float g, float b, float a) {
        Matrix4x4 mvp = mdl.Multiply(view).Multiply(proj);
        struct U { Matrix4x4 MVP; Matrix4x4 Model; float C[4]; } u;
        u.MVP = mvp; u.Model = mdl;
        u.C[0]=r; u.C[1]=g; u.C[2]=b; u.C[3]=a;
        SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));
    };

    auto bindMesh = [&](SDL_GPUBuffer* vb, SDL_GPUBuffer* ib) {
        SDL_GPUBufferBinding v = {}; v.buffer = vb; v.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &v, 1);
        SDL_GPUBufferBinding i = {}; i.buffer = ib; i.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &i, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    };

    auto drawMesh = [&](SDL_GPUBuffer* vb, SDL_GPUBuffer* ib, uint32_t ic,
                        const Matrix4x4& mdl, float r, float g, float b, float a) {
        bindMesh(vb, ib);
        pushDraw(mdl, r, g, b, a);
        SDL_DrawGPUIndexedPrimitives(renderPass, ic, 1, 0, 0, 0);
    };

    // Identity rotation (for Y-aligned shapes and spheres)
    Matrix4x4 iden = Matrix4x4::Identity();
    // Rotate default +Y shape to point along +X or +Z (apex outward)
    Matrix4x4 toX = Matrix4x4::RotationZ(-1.570796f);  // +Y → +X
    Matrix4x4 toZ = Matrix4x4::RotationX(1.570796f);   // +Y → +Z

    auto getRot = [&](int axis) -> const Matrix4x4& {
        if (axis == 0) return toX;
        if (axis == 2) return toZ;
        return iden;
    };

    auto axisColor = [&](int axis, float& r, float& g, float& b) {
        r = (axis==0)?1.0f:0.15f;
        g = (axis==1)?1.0f:0.15f;
        b = (axis==2)?1.0f:0.20f;
        if (axis==2) g = 0.40f;
    };

    // ── MOVE: cylinder shaft + cone tip per axis, sphere center ──
    if (tool == ToolMove) {
        float cs = 0.10f;
        drawMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB, m_Pipeline.SphereIndexCount,
                 model(cs, cs, cs, iden, wp.X, wp.Y, wp.Z), 0.9f, 0.9f, 0.9f, 1.0f);

        float gap = 0.06f;
        float shaftLen = 0.60f;
        float coneH = 0.16f;

        for (int a = 0; a < 3; a++) {
            float he = (a==0)?hx:(a==1)?hy:hz;
            float r, g, b; axisColor(a, r, g, b);
            const Matrix4x4& rot = getRot(a);
            float dx = (a==0)?1:0, dy = (a==1)?1:0, dz = (a==2)?1:0;

            float shaftMid = he + gap + shaftLen * 0.5f;
            float coneMid  = he + gap + shaftLen + coneH * 0.5f;

            // Shaft cylinder (radius 0.04, length shaftLen, along Y by default)
            drawMesh(m_Pipeline.CylinderVB, m_Pipeline.CylinderIB, m_Pipeline.CylinderIndexCount,
                     model(0.04f, shaftLen, 0.04f, rot,
                           wp.X + dx*shaftMid, wp.Y + dy*shaftMid, wp.Z + dz*shaftMid),
                     r*0.65f, g*0.65f, b*0.65f, 1.0f);

            // Cone arrow tip (radius 0.10, height coneH, apex along +Y by default)
            drawMesh(m_Pipeline.ConeVB, m_Pipeline.ConeIB, m_Pipeline.ConeIndexCount,
                     model(0.10f, coneH, 0.10f, rot,
                           wp.X + dx*coneMid, wp.Y + dy*coneMid, wp.Z + dz*coneMid),
                     r, g, b, 1.0f);
        }
    }
    // ── ROTATE: dotted ring of spheres per axis ──
    else if (tool == ToolRotate) {
        float cs = 0.09f;
        drawMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB, m_Pipeline.SphereIndexCount,
                 model(cs, cs, cs, iden, wp.X, wp.Y, wp.Z), 0.9f, 0.9f, 0.9f, 1.0f);

        float ringR = 0.80f;
        int dots = 18;
        float dotSize = 0.035f;

        for (int a = 0; a < 3; a++) {
            float r, g, b; axisColor(a, r, g, b);
            for (int d = 0; d < dots; d++) {
                float ang = (float)d / dots * 6.283185f;
                float px, py, pz;
                if (a == 0) { px = 0;                    py = cosf(ang)*ringR; pz = sinf(ang)*ringR; }
                else if (a == 1) { px = cosf(ang)*ringR; py = 0;                pz = sinf(ang)*ringR; }
                else { px = cosf(ang)*ringR;             py = sinf(ang)*ringR;  pz = 0; }
                drawMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB, m_Pipeline.SphereIndexCount,
                         model(dotSize, dotSize, dotSize, iden, wp.X+px, wp.Y+py, wp.Z+pz),
                         r, g, b, 0.85f);
            }
        }
    }
    // ── SCALE: cylinder lines + endpoint spheres + center sphere ──
    else if (tool == ToolScale) {
        float cs = 0.10f;
        drawMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB, m_Pipeline.SphereIndexCount,
                 model(cs, cs, cs, iden, wp.X, wp.Y, wp.Z), 0.9f, 0.9f, 0.9f, 1.0f);

        float gap = 0.06f;
        float endDist = 0.75f;

        for (int a = 0; a < 3; a++) {
            float he = (a==0)?hx:(a==1)?hy:hz;
            float r, g, b; axisColor(a, r, g, b);
            const Matrix4x4& rot = getRot(a);
            float dx = (a==0)?1:0, dy = (a==1)?1:0, dz = (a==2)?1:0;

            float lineLen = endDist - gap;
            float lineMid = he + gap + lineLen * 0.5f;
            float endPos  = he + endDist;

            // Connecting line
            drawMesh(m_Pipeline.CylinderVB, m_Pipeline.CylinderIB, m_Pipeline.CylinderIndexCount,
                     model(0.025f, lineLen, 0.025f, rot,
                           wp.X + dx*lineMid, wp.Y + dy*lineMid, wp.Z + dz*lineMid),
                     r*0.5f, g*0.5f, b*0.5f, 0.6f);

            // Endpoint sphere
            float es = 0.11f;
            drawMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB, m_Pipeline.SphereIndexCount,
                     model(es, es, es, iden, wp.X + dx*endPos, wp.Y + dy*endPos, wp.Z + dz*endPos),
                     r, g, b, 1.0f);
        }
    }

    // Restore scene light UBO
    SDL_PushGPUFragmentUniformData(cmd, 0, &lightData, sizeof(lightData));
}

Matrix4x4 Renderer::RenderShadowPass(SDL_GPUCommandBuffer* cmd,
                                     LightSource* shadowLight,
                                     const std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    if (!shadowLight || !m_Pipeline.ShadowMap || !m_Pipeline.ShadowPipeline) {
        return Matrix4x4::Identity();
    }

    // Compute scene bounds (min/max of all 3D object world positions)
    Vector3D bMin(1e9f, 1e9f, 1e9f), bMax(-1e9f, -1e9f, -1e9f);
    for (const auto& obj : sceneObjects) {
        if (!obj->Is3D) continue;
        Vector3D pos = obj->GetWorldPosition();
        Vector3D scl = obj->GetWorldScale();
        float he = (obj->ClassName == "LightSource") ? 0.15f : 1.0f;
        bMin.X = std::min(bMin.X, pos.X - scl.X * 0.5f * he);
        bMin.Y = std::min(bMin.Y, pos.Y - scl.Y * 0.5f * he);
        bMin.Z = std::min(bMin.Z, pos.Z - scl.Z * 0.5f * he);
        bMax.X = std::max(bMax.X, pos.X + scl.X * 0.5f * he);
        bMax.Y = std::max(bMax.Y, pos.Y + scl.Y * 0.5f * he);
        bMax.Z = std::max(bMax.Z, pos.Z + scl.Z * 0.5f * he);
    }
    Vector3D center((bMin.X + bMax.X) * 0.5f, (bMin.Y + bMax.Y) * 0.5f, (bMin.Z + bMax.Z) * 0.5f);

    Matrix4x4 lightView, lightProj;

    if (shadowLight->Type == LightType::Directional) {
        // Orthographic projection covering scene bounds
        Vector3D dir = shadowLight->Direction;
        float dlen = sqrtf(dir.X*dir.X + dir.Y*dir.Y + dir.Z*dir.Z);
        if (dlen > 0.001f) { dir.X /= dlen; dir.Y /= dlen; dir.Z /= dlen; }
        else { dir = Vector3D(0, -1, 0); }

        Vector3D lightPos = Vector3D(center.X - dir.X * 20.0f, center.Y - dir.Y * 20.0f, center.Z - dir.Z * 20.0f);
        Vector3D up(0, 1, 0);
        if (fabsf(dir.Y) > 0.99f) up = Vector3D(1, 0, 0);

        lightView = Matrix4x4::LookAt(lightPos, center, up);

        float orthoSize = 15.0f;
        lightProj = Matrix4x4::Orthographic(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 50.0f);
    }
    else if (shadowLight->Type == LightType::Spot) {
        // Perspective projection with spot cone angle
        Vector3D pos = shadowLight->GetWorldPosition();
        Vector3D dir = shadowLight->Direction;
        float dlen = sqrtf(dir.X*dir.X + dir.Y*dir.Y + dir.Z*dir.Z);
        if (dlen > 0.001f) { dir.X /= dlen; dir.Y /= dlen; dir.Z /= dlen; }
        else { dir = Vector3D(0, -1, 0); }

        Vector3D up(0, 1, 0);
        if (fabsf(dir.Y) > 0.99f) up = Vector3D(1, 0, 0);

        lightView = Matrix4x4::LookAt(pos, Vector3D(pos.X + dir.X, pos.Y + dir.Y, pos.Z + dir.Z), up);
        float fovRad = shadowLight->SpotAngleOuter * 2.0f * 3.14159265f / 180.0f;
        lightProj = Matrix4x4::Perspective(fovRad * 180.0f / 3.14159265f, 1.0f, 0.1f, shadowLight->Range);
    }
    else {
        // Point lights not supported for shadows
        return Matrix4x4::Identity();
    }

    Matrix4x4 lightViewProj = lightView.Multiply(lightProj);

    // Begin shadow render pass (depth-only, no color targets)
    SDL_GPUDepthStencilTargetInfo shadowDepth = {};
    shadowDepth.texture = m_Pipeline.ShadowMap;
    shadowDepth.clear_depth = 1.0f;
    shadowDepth.load_op = SDL_GPU_LOADOP_CLEAR;
    shadowDepth.store_op = SDL_GPU_STOREOP_STORE;
    shadowDepth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    shadowDepth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    shadowDepth.cycle = true;

    SDL_GPURenderPass* shadowPass = SDL_BeginGPURenderPass(cmd, nullptr, 0, &shadowDepth);
    if (!shadowPass) {
        return Matrix4x4::Identity();
    }

    // Set viewport to full shadow map size
    SDL_GPUViewport vp = {};
    vp.min_depth = 0.0f; vp.max_depth = 1.0f;
    vp.x = 0.0f; vp.y = 0.0f;
    vp.w = 2048.0f; vp.h = 2048.0f;
    SDL_SetGPUViewport(shadowPass, &vp);

    // Bind shadow pipeline
    SDL_BindGPUGraphicsPipeline(shadowPass, m_Pipeline.ShadowPipeline);
    // Render all 3D objects (depth-only, MVP = LightViewProj * Model)
    for (const auto& obj : sceneObjects) {
        if (!obj->Is3D) continue;

        // Recursively render object and children
        std::function<void(GameObject*)> renderDepth = [&](GameObject* o) {
            if (!o || !o->Is3D) return;
            if (o->ClassName == "LightSource") return;
            if (o->ClassName == "Sun") return;

            Vector3D wp = o->GetWorldPosition();
            Vector3D wr = o->GetWorldRotation();
            Vector3D ws = o->GetWorldScale();
            float sx = ws.X, sy = ws.Y, sz = ws.Z;
            if (o->ClassName == "LightSource") { sx *= 0.15f; sy *= 0.15f; sz *= 0.15f; }

            float rotX = wr.X * 3.14159265f / 180.0f;
            float rotY = wr.Y * 3.14159265f / 180.0f;
            float rotZ = wr.Z * 3.14159265f / 180.0f;
            float offRotX = o->RotationOffset.X * 3.14159265f / 180.0f;
            float offRotY = o->RotationOffset.Y * 3.14159265f / 180.0f;
            float offRotZ = o->RotationOffset.Z * 3.14159265f / 180.0f;

            Matrix4x4 modelMat = Matrix4x4::Translation(o->PositionOffset.X, o->PositionOffset.Y, o->PositionOffset.Z)
                .Multiply(Matrix4x4::RotationZ(offRotZ))
                .Multiply(Matrix4x4::RotationX(offRotX))
                .Multiply(Matrix4x4::RotationY(offRotY))
                .Multiply(Matrix4x4::Scale(sx, sy, sz))
                .Multiply(Matrix4x4::RotationZ(rotZ))
                .Multiply(Matrix4x4::RotationX(rotX))
                .Multiply(Matrix4x4::RotationY(rotY))
                .Multiply(Matrix4x4::Translation(wp.X, wp.Y, wp.Z));

            Matrix4x4 mvp = modelMat.Multiply(lightViewProj);

            struct U { Matrix4x4 MVP; Matrix4x4 Model; float C[4]; } u;
            u.MVP = mvp;
            u.Model = modelMat;
            u.C[0] = 0; u.C[1] = 0; u.C[2] = 0; u.C[3] = 0;

            SDL_GPUBufferBinding vb = {};
            vb.buffer = o->CustomVertexBuffer ? o->CustomVertexBuffer.get() : m_Pipeline.VertexBuffer;
            vb.offset = 0;
            SDL_BindGPUVertexBuffers(shadowPass, 0, &vb, 1);

            SDL_GPUBufferBinding ib = {};
            ib.buffer = o->CustomIndexBuffer ? o->CustomIndexBuffer.get() : m_Pipeline.IndexBuffer;
            ib.offset = 0;
            SDL_BindGPUIndexBuffer(shadowPass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

            SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));

            if (!o->CustomSubMeshes.empty()) {
                for (auto& sm : o->CustomSubMeshes) {
                    SDL_GPUTextureSamplerBinding bindings[2] = {};
                    bindings[0].texture = m_Pipeline.ShadowMap;
                    bindings[0].sampler = m_Pipeline.ShadowSampler;
                    SDL_GPUTexture* diffuseTex = sm.Texture ? sm.Texture.get() : m_Pipeline.DefaultTexture;
                    bindings[1].texture = diffuseTex;
                    bindings[1].sampler = m_Pipeline.DefaultSampler;
                    SDL_BindGPUFragmentSamplers(shadowPass, 0, bindings, 2);
                    SDL_DrawGPUIndexedPrimitives(shadowPass, sm.IndexCount, 1, sm.IndexStart, 0, 0);
                }
            } else {
                SDL_GPUTextureSamplerBinding bindings[2] = {};
                bindings[0].texture = m_Pipeline.ShadowMap;
                bindings[0].sampler = m_Pipeline.ShadowSampler;
                bindings[1].texture = m_Pipeline.DefaultTexture;
                bindings[1].sampler = m_Pipeline.DefaultSampler;
                SDL_BindGPUFragmentSamplers(shadowPass, 0, bindings, 2);
                uint32_t idxCount = o->CustomIndexBuffer ? o->CustomIndexCount : 36;
                SDL_DrawGPUIndexedPrimitives(shadowPass, idxCount, 1, 0, 0, 0);
            }

            for (auto& child : o->Children) {
                renderDepth(child.get());
            }
        };

        renderDepth(obj.get());
    }

    SDL_EndGPURenderPass(shadowPass);

    return lightViewProj;
}

void Renderer::RenderSun(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                         Sun* sun, const Matrix4x4& view, const Matrix4x4& proj) {
    if (!sun || !sun->IsAboveHorizon()) return;

    // Push neutral light for unlit sun rendering
    LightUBO sunLight = {};
    sunLight.lightCount = 0;
    sunLight.ambientIntensity = 1.0f;
    SDL_PushGPUFragmentUniformData(cmd, 0, &sunLight, sizeof(sunLight));

    Vector3D sunPos = sun->GetSunPosition(300.0f);

    auto bindMesh = [&](SDL_GPUBuffer* vb, SDL_GPUBuffer* ib) {
        SDL_GPUBufferBinding v = {}; v.buffer = vb; v.offset = 0;
        SDL_BindGPUVertexBuffers(renderPass, 0, &v, 1);
        SDL_GPUBufferBinding i = {}; i.buffer = ib; i.offset = 0;
        SDL_BindGPUIndexBuffer(renderPass, &i, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    };

    auto drawSphere = [&](float scale, float r, float g, float b, float a) {
        Matrix4x4 model = Matrix4x4::Scale(scale, scale, scale)
            .Multiply(Matrix4x4::Translation(sunPos.X, sunPos.Y, sunPos.Z));
        Matrix4x4 mvp = model.Multiply(view).Multiply(proj);
        struct U { Matrix4x4 MVP; Matrix4x4 Model; float C[4]; } u;
        u.MVP = mvp; u.Model = model;
        u.C[0] = r; u.C[1] = g; u.C[2] = b; u.C[3] = a;
        bindMesh(m_Pipeline.SphereVB, m_Pipeline.SphereIB);
        SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));
        SDL_DrawGPUIndexedPrimitives(renderPass, m_Pipeline.SphereIndexCount, 1, 0, 0, 0);
    };

    // Glowing sun: core + 3 glow layers for soft edges
    drawSphere(6.0f,  1.0f, 1.0f, 0.95f, 1.0f);   // core - solid warm white
    drawSphere(10.0f, 1.0f, 0.95f, 0.8f, 0.5f);   // inner glow
    drawSphere(16.0f, 1.0f, 0.9f, 0.7f, 0.25f);   // mid glow
    drawSphere(24.0f, 1.0f, 0.85f, 0.6f, 0.12f);  // outer glow
    drawSphere(35.0f, 1.0f, 0.8f, 0.5f, 0.05f);   // far glow halo
}

void Renderer::RenderSky(SDL_GPURenderPass* renderPass, SDL_GPUCommandBuffer* cmd,
                         float camX, float camY, float camZ,
                         const Matrix4x4& view, const Matrix4x4& proj) {
    if (!m_Pipeline.SkyPipeline || !m_Pipeline.SkyTexture || !m_Pipeline.SkySampler) return;

    // Bind sky pipeline
    SDL_BindGPUGraphicsPipeline(renderPass, m_Pipeline.SkyPipeline);

    // Bind cube vertex/index buffers
    SDL_GPUBufferBinding vb = {};
    vb.buffer = m_Pipeline.VertexBuffer;
    vb.offset = 0;
    SDL_BindGPUVertexBuffers(renderPass, 0, &vb, 1);

    SDL_GPUBufferBinding ib = {};
    ib.buffer = m_Pipeline.IndexBuffer;
    ib.offset = 0;
    SDL_BindGPUIndexBuffer(renderPass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    // Bind shadow sampler (s0) + sky sampler (s1) - both required by pipeline
    SDL_GPUTextureSamplerBinding bindings[2] = {};
    bindings[0].texture = m_Pipeline.ShadowMap;
    bindings[0].sampler = m_Pipeline.ShadowSampler;
    bindings[1].texture = m_Pipeline.SkyTexture;
    bindings[1].sampler = m_Pipeline.SkySampler;
    SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);

    // Large cube centered on camera (500 units, so it's always far away)
    float skySize = 500.0f;
    Matrix4x4 model = Matrix4x4::Scale(skySize, skySize, skySize)
        .Multiply(Matrix4x4::Translation(camX, camY, camZ));
    Matrix4x4 mvp = model.Multiply(view).Multiply(proj);

    struct U { Matrix4x4 MVP; Matrix4x4 Model; float C[4]; } u;
    u.MVP = mvp;
    u.Model = model;
    u.C[0] = 0; u.C[1] = 0; u.C[2] = 0; u.C[3] = 0;
    SDL_PushGPUVertexUniformData(cmd, 0, &u, sizeof(u));

    // Push neutral light UBO (sky shader doesn't use it, but pipeline requires the binding)
    LightUBO neutralLight = {};
    neutralLight.lightCount = 0;
    neutralLight.ambientIntensity = 1.0f;
    neutralLight.HasShadow = 0;
    neutralLight.LightViewProj = Matrix4x4::Identity();
    SDL_PushGPUFragmentUniformData(cmd, 0, &neutralLight, sizeof(neutralLight));

    SDL_DrawGPUIndexedPrimitives(renderPass, 36, 1, 0, 0, 0);
}
