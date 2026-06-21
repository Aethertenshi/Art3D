#pragma once
#include <vector>
#include <cmath>
#include "Vertex.h"

namespace MeshBuilder {

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

// Cone: apex at +Y, base at -Y
inline Mesh Cone(float radius, float height, int segments = 16) {
    Mesh m;
    float halfH = height * 0.5f;

    Vertex apex = { 0, halfH, 0, 1,1,1,1, 0,1,0, 0,0 };
    m.vertices.push_back(apex);

    Vertex baseCenter = { 0, -halfH, 0, 1,1,1,1, 0,-1,0, 0,0 };
    m.vertices.push_back(baseCenter);

    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 6.2831853f;
        float nx = cosf(angle);
        float nz = sinf(angle);
        float coneNx = nx * height / sqrtf(height*height + radius*radius);
        float coneNy = radius / sqrtf(height*height + radius*radius);
        float coneNz = nz * height / sqrtf(height*height + radius*radius);
        float len = sqrtf(coneNx*coneNx + coneNy*coneNy + coneNz*coneNz);
        coneNx /= len; coneNy /= len; coneNz /= len;

        m.vertices.push_back({ nx*radius, -halfH, nz*radius, 1,1,1,1, coneNx, coneNy, coneNz, 0,0 });
    }

    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        int a = i + 2;
        int b = next + 2;
        m.indices.push_back(0);
        m.indices.push_back(a);
        m.indices.push_back(b);
    }

    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        int a = i + 2;
        int b = next + 2;
        m.indices.push_back(1);
        m.indices.push_back(b);
        m.indices.push_back(a);
    }

    return m;
}

// Cylinder: axis along Y, from -halfH to +halfH
inline Mesh Cylinder(float radius, float height, int segments = 16) {
    Mesh m;
    float halfH = height * 0.5f;

    Vertex topCenter = { 0, halfH, 0, 1,1,1,1, 0,1,0, 0,0 };
    Vertex bottomCenter = { 0, -halfH, 0, 1,1,1,1, 0,-1,0, 0,0 };
    m.vertices.push_back(topCenter);
    m.vertices.push_back(bottomCenter);

    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 6.2831853f;
        float x = cosf(angle) * radius;
        float z = sinf(angle) * radius;
        m.vertices.push_back({ x, halfH, z, 1,1,1,1, 0,1,0, 0,0 });
    }
    for (int i = 0; i < segments; i++) {
        float angle = (float)i / segments * 6.2831853f;
        float x = cosf(angle) * radius;
        float z = sinf(angle) * radius;
        m.vertices.push_back({ x, -halfH, z, 1,1,1,1, 0,-1,0, 0,0 });
    }

    int topStart    = 2;
    int bottomStart = 2 + segments;

    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        m.indices.push_back(0);
        m.indices.push_back(topStart + i);
        m.indices.push_back(topStart + next);
    }
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        m.indices.push_back(1);
        m.indices.push_back(bottomStart + next);
        m.indices.push_back(bottomStart + i);
    }
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        int tl = topStart + i, tr = topStart + next;
        int bl = bottomStart + i, br = bottomStart + next;

        Vector3D edge1(cosf((float)next/segments*6.2831853f)*radius - cosf((float)i/segments*6.2831853f)*radius, 0,
                       sinf((float)next/segments*6.2831853f)*radius - sinf((float)i/segments*6.2831853f)*radius);
        Vector3D edge2(0, -height, 0);
        Vector3D n(edge1.Y*edge2.Z - edge1.Z*edge2.Y, edge1.Z*edge2.X - edge1.X*edge2.Z, edge1.X*edge2.Y - edge1.Y*edge2.X);
        float len = sqrtf(n.X*n.X + n.Y*n.Y + n.Z*n.Z);
        if (len > 1e-6f) { n.X/=len; n.Y/=len; n.Z/=len; }

        m.vertices[tl].nx = n.X; m.vertices[tl].ny = n.Y; m.vertices[tl].nz = n.Z;
        m.vertices[tr].nx = n.X; m.vertices[tr].ny = n.Y; m.vertices[tr].nz = n.Z;
        m.vertices[bl].nx = n.X; m.vertices[bl].ny = n.Y; m.vertices[bl].nz = n.Z;
        m.vertices[br].nx = n.X; m.vertices[br].ny = n.Y; m.vertices[br].nz = n.Z;

        m.indices.push_back(tl); m.indices.push_back(bl); m.indices.push_back(br);
        m.indices.push_back(tl); m.indices.push_back(br); m.indices.push_back(tr);
    }

    return m;
}

// UV Sphere: centered at origin
inline Mesh Sphere(float radius, int latSegs = 12, int lonSegs = 16) {
    Mesh m;

    Vertex top = { 0, radius, 0, 1,1,1,1, 0,1,0, 0,0 };
    m.vertices.push_back(top);
    Vertex bottom = { 0, -radius, 0, 1,1,1,1, 0,-1,0, 0,0 };
    m.vertices.push_back(bottom);

    for (int lat = 1; lat < latSegs; lat++) {
        float phi = 3.14159265f * (float)lat / latSegs;
        float y = cosf(phi) * radius;
        float ringR = sinf(phi) * radius;

        for (int lon = 0; lon < lonSegs; lon++) {
            float theta = 6.2831853f * (float)lon / lonSegs;
            float x = cosf(theta) * ringR;
            float z = sinf(theta) * ringR;
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;
            m.vertices.push_back({ x, y, z, 1,1,1,1, nx, ny, nz, 0,0 });
        }
    }

    for (int lon = 0; lon < lonSegs; lon++) {
        int next = (lon + 1) % lonSegs;
        m.indices.push_back(0);
        m.indices.push_back(2 + lon);
        m.indices.push_back(2 + next);
    }

    for (int lat = 0; lat < latSegs - 2; lat++) {
        int rowA = 2 + lat * lonSegs;
        int rowB = 2 + (lat + 1) * lonSegs;
        for (int lon = 0; lon < lonSegs; lon++) {
            int next = (lon + 1) % lonSegs;
            m.indices.push_back(rowA + lon);
            m.indices.push_back(rowB + lon);
            m.indices.push_back(rowB + next);
            m.indices.push_back(rowA + lon);
            m.indices.push_back(rowB + next);
            m.indices.push_back(rowA + next);
        }
    }

    int lastRow = 2 + (latSegs - 2) * lonSegs;
    for (int lon = 0; lon < lonSegs; lon++) {
        int next = (lon + 1) % lonSegs;
        m.indices.push_back(1);
        m.indices.push_back(lastRow + next);
        m.indices.push_back(lastRow + lon);
    }

    return m;
}

// Wedge: right-angled triangular prism along X/Y, thick in Z, like a ramp
inline Mesh Wedge(float sx, float sy, float sz) {
    Mesh m;
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;

    m.vertices.push_back({ -hx, -hy, -hz, 1,1,1,1, 0,0,-1, 0,0 }); // 0: back-bottom-left
    m.vertices.push_back({  hx, -hy, -hz, 1,1,1,1, 0,0,-1, 0,0 }); // 1: back-bottom-right
    m.vertices.push_back({  hx,  hy, -hz, 1,1,1,1, 0,0,-1, 0,0 }); // 2: back-top-right
    m.vertices.push_back({ -hx, -hy,  hz, 1,1,1,1, 0,0, 1, 0,0 }); // 3: front-bottom-left
    m.vertices.push_back({  hx, -hy,  hz, 1,1,1,1, 0,0, 1, 0,0 }); // 4: front-bottom-right
    m.vertices.push_back({  hx,  hy,  hz, 1,1,1,1, 0,0, 1, 0,0 }); // 5: front-top-right

    // Bottom face
    m.vertices[0].nx=0;m.vertices[0].ny=-1;m.vertices[0].nz=0;
    m.vertices[1].nx=0;m.vertices[1].ny=-1;m.vertices[1].nz=0;
    m.vertices[3].nx=0;m.vertices[3].ny=-1;m.vertices[3].nz=0;
    m.vertices[4].nx=0;m.vertices[4].ny=-1;m.vertices[4].nz=0;

    // Triangles: back, front, bottom, sloped, left, right
    m.indices.insert(m.indices.end(), { 0,1,2 }); // back
    m.indices.insert(m.indices.end(), { 3,5,4 }); // front
    m.indices.insert(m.indices.end(), { 0,3,4, 0,4,1 }); // bottom
    m.indices.insert(m.indices.end(), { 2,5,3, 2,3,0 }); // sloped
    m.indices.insert(m.indices.end(), { 1,4,5, 1,5,2 }); // right

    return m;
}

} // namespace MeshBuilder
