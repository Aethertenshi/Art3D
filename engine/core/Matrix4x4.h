#pragma once
#include <cmath>
#include <algorithm>
#include "Vector3D.h"

struct Matrix4x4 {
    float m[4][4] = {0};

    static Matrix4x4 Identity() {
        Matrix4x4 mat;
        mat.m[0][0] = 1.0f;
        mat.m[1][1] = 1.0f;
        mat.m[2][2] = 1.0f;
        mat.m[3][3] = 1.0f;
        return mat;
    }

    static Matrix4x4 Perspective(float fovDeg, float aspect, float nearZ, float farZ) {
        Matrix4x4 mat;
        float fovRad = fovDeg * 3.14159265f / 180.0f;
        float tanHalfFov = tanf(fovRad / 2.0f);
        mat.m[0][0] = 1.0f / (aspect * tanHalfFov);
        mat.m[1][1] = 1.0f / tanHalfFov;
        mat.m[2][2] = farZ / (farZ - nearZ);
        mat.m[2][3] = 1.0f;
        mat.m[3][2] = -(farZ * nearZ) / (farZ - nearZ);
        return mat;
    }

    static Matrix4x4 RotationY(float angleRad) {
        Matrix4x4 mat = Identity();
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        mat.m[0][0] = c; mat.m[0][2] = -s;
        mat.m[2][0] = s; mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 RotationX(float angleRad) {
        Matrix4x4 mat = Identity();
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        mat.m[1][1] = c; mat.m[1][2] = s;
        mat.m[2][1] = -s; mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 RotationZ(float angleRad) {
        Matrix4x4 mat = Identity();
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        mat.m[0][0] = c; mat.m[0][1] = s;
        mat.m[1][0] = -s; mat.m[1][1] = c;
        return mat;
    }

    static Matrix4x4 Scale(float x, float y, float z) {
        Matrix4x4 mat = Identity();
        mat.m[0][0] = x;
        mat.m[1][1] = y;
        mat.m[2][2] = z;
        return mat;
    }

    static Matrix4x4 Translation(float x, float y, float z) {
        Matrix4x4 mat = Identity();
        mat.m[3][0] = x;
        mat.m[3][1] = y;
        mat.m[3][2] = z;
        return mat;
    }

    static Matrix4x4 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ) {
        Matrix4x4 mat;
        mat.m[0][0] = 2.0f / (right - left);
        mat.m[1][1] = 2.0f / (top - bottom);
        mat.m[2][2] = 1.0f / (farZ - nearZ);
        mat.m[3][0] = -(right + left) / (right - left);
        mat.m[3][1] = -(top + bottom) / (top - bottom);
        mat.m[3][2] = -nearZ / (farZ - nearZ);
        mat.m[3][3] = 1.0f;
        return mat;
    }

    static Matrix4x4 LookAt(const Vector3D& eye, const Vector3D& target, const Vector3D& up) {
        Vector3D f = target - eye;
        float fLen = sqrtf(f.X * f.X + f.Y * f.Y + f.Z * f.Z);
        if (fLen < 0.0001f) return Identity();
        f.X /= fLen; f.Y /= fLen; f.Z /= fLen;

        Vector3D s = { up.Y * f.Z - up.Z * f.Y, up.Z * f.X - up.X * f.Z, up.X * f.Y - up.Y * f.X };
        float sLen = sqrtf(s.X*s.X + s.Y*s.Y + s.Z*s.Z);
        if (sLen < 0.0001f) return Identity();
        s.X /= sLen; s.Y /= sLen; s.Z /= sLen;

        Vector3D u = { f.Y * s.Z - f.Z * s.Y, f.Z * s.X - f.X * s.Z, f.X * s.Y - f.Y * s.X };

        Matrix4x4 mat;
        mat.m[0][0] = s.X; mat.m[0][1] = u.X; mat.m[0][2] = f.X; mat.m[0][3] = 0.0f;
        mat.m[1][0] = s.Y; mat.m[1][1] = u.Y; mat.m[1][2] = f.Y; mat.m[1][3] = 0.0f;
        mat.m[2][0] = s.Z; mat.m[2][1] = u.Z; mat.m[2][2] = f.Z; mat.m[2][3] = 0.0f;
        mat.m[3][0] = -(s.X * eye.X + s.Y * eye.Y + s.Z * eye.Z);
        mat.m[3][1] = -(u.X * eye.X + u.Y * eye.Y + u.Z * eye.Z);
        mat.m[3][2] = -(f.X * eye.X + f.Y * eye.Y + f.Z * eye.Z);
        mat.m[3][3] = 1.0f;
        return mat;
    }

    Matrix4x4 Multiply(const Matrix4x4& other) const {
        Matrix4x4 result;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                result.m[r][c] =
                    m[r][0] * other.m[0][c] +
                    m[r][1] * other.m[1][c] +
                    m[r][2] * other.m[2][c] +
                    m[r][3] * other.m[3][c];
            }
        }
        return result;
    }

    Matrix4x4 Transpose() const {
        Matrix4x4 result;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                result.m[c][r] = m[r][c];
            }
        }
        return result;
    }
};
