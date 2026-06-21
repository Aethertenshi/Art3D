#include "OBJLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>

bool LoadObj(const std::string& filepath, std::vector<Vertex>& outVertices, std::vector<uint32_t>& outIndices, Vector3D& outCenter) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
        return false;
    }

    std::vector<Vector3D> temp_positions;
    std::vector<Vector3D> temp_normals;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;

        if (prefix == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            temp_positions.push_back(Vector3D(x, y, z));
        } else if (prefix == "vn") {
            float nx, ny, nz;
            ss >> nx >> ny >> nz;
            temp_normals.push_back(Vector3D(nx, ny, nz));
        } else if (prefix == "f") {
            std::vector<std::string> faceTokens;
            std::string token;
            while (ss >> token) faceTokens.push_back(token);

            if (faceTokens.size() < 3) continue;

            auto parseVertexToken = [&](const std::string& tok) -> Vertex {
                std::stringstream tokSS(tok);
                std::string vStr, vtStr, vnStr;
                std::getline(tokSS, vStr, '/');
                std::getline(tokSS, vtStr, '/');
                std::getline(tokSS, vnStr, '/');

                int vIdx = 0;
                try { vIdx = std::stoi(vStr) - 1; } catch (...) { vIdx = -1; }
                if (vIdx < 0 && !temp_positions.empty()) vIdx = (int)temp_positions.size() + vIdx + 1;

                Vertex v = {};
                if (vIdx >= 0 && vIdx < (int)temp_positions.size()) {
                    v.x = temp_positions[vIdx].X;
                    v.y = temp_positions[vIdx].Y;
                    v.z = temp_positions[vIdx].Z;
                }
                v.r = 0.8f; v.g = 0.8f; v.b = 0.8f; v.a = 1.0f;
                v.u = 0.0f; v.v = 0.0f;

                int vnIdx = -1;
                if (!vnStr.empty()) {
                    try { vnIdx = std::stoi(vnStr) - 1; } catch (...) { vnIdx = -1; }
                }
                if (vnIdx >= 0 && vnIdx < (int)temp_normals.size()) {
                    v.nx = temp_normals[vnIdx].X;
                    v.ny = temp_normals[vnIdx].Y;
                    v.nz = temp_normals[vnIdx].Z;
                } else {
                    v.nx = 0.0f; v.ny = 0.0f; v.nz = 0.0f;
                }
                return v;
            };

            std::vector<Vertex> faceVertices;
            for (const auto& tok : faceTokens) faceVertices.push_back(parseVertexToken(tok));

            for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
                Vertex v0 = faceVertices[0];
                Vertex v1 = faceVertices[i + 1];
                Vertex v2 = faceVertices[i];

                if (v0.nx == 0.0f && v0.ny == 0.0f && v0.nz == 0.0f &&
                    v1.nx == 0.0f && v1.ny == 0.0f && v1.nz == 0.0f &&
                    v2.nx == 0.0f && v2.ny == 0.0f && v2.nz == 0.0f) {

                    Vector3D edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
                    Vector3D edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
                    Vector3D norm(
                        edge1.Y * edge2.Z - edge1.Z * edge2.Y,
                        edge1.Z * edge2.X - edge1.X * edge2.Z,
                        edge1.X * edge2.Y - edge1.Y * edge2.X
                    );
                    float len = sqrtf(norm.X * norm.X + norm.Y * norm.Y + norm.Z * norm.Z);
                    if (len > 1e-6f) { norm.X /= len; norm.Y /= len; norm.Z /= len; }
                    else { norm = Vector3D(0.0f, 1.0f, 0.0f); }
                    v0.nx = v1.nx = v2.nx = norm.X;
                    v0.ny = v1.ny = v2.ny = norm.Y;
                    v0.nz = v1.nz = v2.nz = norm.Z;
                }

                uint32_t startIdx = (uint32_t)outVertices.size();
                outVertices.push_back(v0);
                outVertices.push_back(v1);
                outVertices.push_back(v2);
                outIndices.push_back(startIdx);
                outIndices.push_back(startIdx + 1);
                outIndices.push_back(startIdx + 2);
            }
        }
    }

    if (outVertices.empty()) return false;

    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;

    for (const auto& v : outVertices) {
        if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
        if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
    }

    outCenter.X = (minX + maxX) * 0.5f;
    outCenter.Y = (minY + maxY) * 0.5f;
    outCenter.Z = (minZ + maxZ) * 0.5f;

    for (auto& v : outVertices) {
        v.x -= outCenter.X;
        v.y -= outCenter.Y;
        v.z -= outCenter.Z;
    }

    return true;
}
