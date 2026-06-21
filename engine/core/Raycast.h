#pragma once
#include "core/Vector3D.h"
#include <cmath>
#include <algorithm>

inline bool RayAABBIntersect(
    const Vector3D& rayOrigin, const Vector3D& rayDir, float rayLength,
    const Vector3D& boxCenter, const Vector3D& boxHalfSize,
    float& tNear, float& tFar
) {
    float tMin = -1e9f;
    float tMax = 1e9f;

    if (std::abs(rayDir.X) < 1e-6f) {
        if (rayOrigin.X < boxCenter.X - boxHalfSize.X || rayOrigin.X > boxCenter.X + boxHalfSize.X) return false;
    } else {
        float invD = 1.0f / rayDir.X;
        float t1 = (boxCenter.X - boxHalfSize.X - rayOrigin.X) * invD;
        float t2 = (boxCenter.X + boxHalfSize.X - rayOrigin.X) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    if (std::abs(rayDir.Y) < 1e-6f) {
        if (rayOrigin.Y < boxCenter.Y - boxHalfSize.Y || rayOrigin.Y > boxCenter.Y + boxHalfSize.Y) return false;
    } else {
        float invD = 1.0f / rayDir.Y;
        float t1 = (boxCenter.Y - boxHalfSize.Y - rayOrigin.Y) * invD;
        float t2 = (boxCenter.Y + boxHalfSize.Y - rayOrigin.Y) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    if (std::abs(rayDir.Z) < 1e-6f) {
        if (rayOrigin.Z < boxCenter.Z - boxHalfSize.Z || rayOrigin.Z > boxCenter.Z + boxHalfSize.Z) return false;
    } else {
        float invD = 1.0f / rayDir.Z;
        float t1 = (boxCenter.Z - boxHalfSize.Z - rayOrigin.Z) * invD;
        float t2 = (boxCenter.Z + boxHalfSize.Z - rayOrigin.Z) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }

    tNear = tMin;
    tFar = tMax;
    return true;
}
