#pragma once
#include <cmath>
#include <algorithm>

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
        mat.m[0][0] = c;
        mat.m[0][2] = -s;
        mat.m[2][0] = s;
        mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 RotationX(float angleRad) {
        Matrix4x4 mat = Identity();
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        mat.m[1][1] = c;
        mat.m[1][2] = s;
        mat.m[2][1] = -s;
        mat.m[2][2] = c;
        return mat;
    }

    static Matrix4x4 RotationZ(float angleRad) {
        Matrix4x4 mat = Identity();
        float c = cosf(angleRad);
        float s = sinf(angleRad);
        mat.m[0][0] = c;
        mat.m[0][1] = s;
        mat.m[1][0] = -s;
        mat.m[1][1] = c;
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
