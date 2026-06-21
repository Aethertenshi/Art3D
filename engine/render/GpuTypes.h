#pragma once
#include "core/Vector3D.h"
#include "core/Matrix4x4.h"

struct GPULight {
    float pos[4];
    float dir[4];
    float color[4];
    float spotAngles[2];
    float pad[2];
};

struct LightUBO {
    GPULight lights[8];
    int lightCount;
    float ambientIntensity;
    float _shadowPad0[2];      // align LightViewProj to 16-byte boundary
    Matrix4x4 LightViewProj;   // light's view-projection for shadow sampling
    int HasShadow;             // 0 = no shadow, 1 = shadow active
    float _shadowPad1[3];      // pad to 16 bytes
};
