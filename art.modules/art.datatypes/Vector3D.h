#pragma once

struct Vector3D
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;

    Vector3D() = default;
    Vector3D(float x, float y, float z) : X(x), Y(y), Z(z) {}

    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(X + other.X, Y + other.Y, Z + other.Z);
    }

    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(X - other.X, Y - other.Y, Z - other.Z);
    }

    Vector3D operator*(float scalar) const {
        return Vector3D(X * scalar, Y * scalar, Z * scalar);
    }
};
