#pragma once
#include "core/GameObject.h"
#include "core/Vector3D.h"
#include "core/Matrix4x4.h"
#include "core/Raycast.h"
#include <vector>
#include <memory>
#include <cmath>

void Collect3DObjects(GameObject* obj, std::vector<GameObject*>& outList);

Vector3D GetFinalWorldCenter(GameObject* obj);

bool IsObjectShadowed(GameObject* target, const std::vector<std::shared_ptr<GameObject>>& sceneObjects, const Vector3D& lightPos);

std::vector<std::shared_ptr<GameObject>> CloneScene(const std::vector<std::shared_ptr<GameObject>>& scene);

GameObject* FindObjectByName(GameObject* root, const std::string& name);

GameObject* FindSelectedInScene(const std::vector<std::shared_ptr<GameObject>>& scene, const std::string& name);

bool RemoveObjectFromHierarchy(GameObject* parent, GameObject* target);

void DeleteObject(GameObject* target, std::vector<std::shared_ptr<GameObject>>& sceneObjects);
