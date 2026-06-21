#pragma once
#include <string>
#include <vector>
#include <memory>
#include "core/GameObject.h"

class BasePart : public GameObject
{
public:
    BasePart(const std::string& name)
        : GameObject(name, Vector3D(0.0f, 0.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f)) {
        ClassName = "BasePart";
    }

    std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<BasePart>(Name);
        CopyTo(clone.get());
        return clone;
    }

    virtual ~BasePart() = default;
};
