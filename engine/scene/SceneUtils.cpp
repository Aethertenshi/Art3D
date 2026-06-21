#include "SceneUtils.h"

void Collect3DObjects(GameObject* obj, std::vector<GameObject*>& outList) {
    if (!obj) return;
    if (obj->Is3D) {
        outList.push_back(obj);
    }
    for (auto& child : obj->Children) {
        Collect3DObjects(child.get(), outList);
    }
}

Vector3D GetFinalWorldCenter(GameObject* obj) {
    if (!obj) return Vector3D(0, 0, 0);
    Vector3D worldPos = obj->GetWorldPosition();
    Vector3D worldRot = obj->GetWorldRotation();
    Vector3D worldScale = obj->GetWorldScale();

    float rotX = worldRot.X * 3.14159265f / 180.0f;
    float rotY = worldRot.Y * 3.14159265f / 180.0f;
    float rotZ = worldRot.Z * 3.14159265f / 180.0f;

    float offRotX = obj->RotationOffset.X * 3.14159265f / 180.0f;
    float offRotY = obj->RotationOffset.Y * 3.14159265f / 180.0f;
    float offRotZ = obj->RotationOffset.Z * 3.14159265f / 180.0f;

    float scaleX = worldScale.X;
    float scaleY = worldScale.Y;
    float scaleZ = worldScale.Z;
    if (obj->ClassName == "LightSource") {
        scaleX *= 0.15f; scaleY *= 0.15f; scaleZ *= 0.15f;
    }

    Matrix4x4 model = Matrix4x4::Translation(obj->PositionOffset.X, obj->PositionOffset.Y, obj->PositionOffset.Z)
        .Multiply(Matrix4x4::RotationZ(offRotZ))
        .Multiply(Matrix4x4::RotationX(offRotX))
        .Multiply(Matrix4x4::RotationY(offRotY))
        .Multiply(Matrix4x4::Scale(scaleX, scaleY, scaleZ))
        .Multiply(Matrix4x4::RotationZ(rotZ))
        .Multiply(Matrix4x4::RotationX(rotX))
        .Multiply(Matrix4x4::RotationY(rotY))
        .Multiply(Matrix4x4::Translation(worldPos.X, worldPos.Y, worldPos.Z));

    return Vector3D(model.m[3][0], model.m[3][1], model.m[3][2]);
}

bool IsObjectShadowed(GameObject* target, const std::vector<std::shared_ptr<GameObject>>& sceneObjects, const Vector3D& lightPos) {
    if (!target) return false;
    Vector3D targetPos = GetFinalWorldCenter(target);
    Vector3D rayDir = lightPos - targetPos;
    float rayLength = std::sqrt(rayDir.X * rayDir.X + rayDir.Y * rayDir.Y + rayDir.Z * rayDir.Z);
    if (rayLength < 0.001f) return false;

    rayDir.X /= rayLength;
    rayDir.Y /= rayLength;
    rayDir.Z /= rayLength;

    std::vector<GameObject*> all3DObjects;
    for (const auto& root : sceneObjects) {
        Collect3DObjects(root.get(), all3DObjects);
    }

    for (GameObject* blocker : all3DObjects) {
        if (blocker == target) continue;
        if (blocker->ClassName == "LightSource") continue;

        Vector3D boxPos = GetFinalWorldCenter(blocker);
        Vector3D boxScale = blocker->GetWorldScale();

        float hx = boxScale.X * 0.5f;
        float hy = boxScale.Y * 0.5f;
        float hz = boxScale.Z * 0.5f;

        float tNear = 0.0f;
        float tFar = 0.0f;
        if (RayAABBIntersect(targetPos, rayDir, rayLength, boxPos, Vector3D(hx, hy, hz), tNear, tFar)) {
            float overlapStart = std::max(tNear, 0.05f);
            float overlapEnd = std::min(tFar, rayLength - 0.05f);
            if (overlapStart < overlapEnd) return true;
        }
    }
    return false;
}

std::vector<std::shared_ptr<GameObject>> CloneScene(const std::vector<std::shared_ptr<GameObject>>& scene) {
    std::vector<std::shared_ptr<GameObject>> clonedScene;
    for (const auto& obj : scene) {
        clonedScene.push_back(obj->Clone());
    }
    return clonedScene;
}

GameObject* FindObjectByName(GameObject* root, const std::string& name) {
    if (!root) return nullptr;
    if (root->Name == name) return root;
    for (auto& child : root->Children) {
        GameObject* found = FindObjectByName(child.get(), name);
        if (found) return found;
    }
    return nullptr;
}

GameObject* FindSelectedInScene(const std::vector<std::shared_ptr<GameObject>>& scene, const std::string& name) {
    for (auto& obj : scene) {
        GameObject* found = FindObjectByName(obj.get(), name);
        if (found) return found;
    }
    return nullptr;
}

bool RemoveObjectFromHierarchy(GameObject* parent, GameObject* target) {
    if (!parent) return false;
    for (auto it = parent->Children.begin(); it != parent->Children.end(); ++it) {
        if (it->get() == target) {
            parent->Children.erase(it);
            return true;
        }
        if (RemoveObjectFromHierarchy(it->get(), target)) {
            return true;
        }
    }
    return false;
}

void DeleteObject(GameObject* target, std::vector<std::shared_ptr<GameObject>>& sceneObjects) {
    if (!target) return;
    for (auto it = sceneObjects.begin(); it != sceneObjects.end(); ++it) {
        if (it->get() == target) {
            sceneObjects.erase(it);
            return;
        }
    }
    for (auto& root : sceneObjects) {
        if (RemoveObjectFromHierarchy(root.get(), target)) {
            return;
        }
    }
}
