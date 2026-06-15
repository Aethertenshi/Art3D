#pragma once
#include <string>
#include "../art.datatypes/GameObject.h"
#include "../art.datatypes/Vector3D.h"

class LightSource : public GameObject
{
public:
    Vector3D Color;
    float Intensity;

    LightSource(const std::string& name)
        : GameObject(name, Vector3D(0.0f, 3.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f)),
          Color(1.0f, 1.0f, 1.0f), Intensity(1.0f)
    {
        ClassName = "LightSource";
    }

    virtual std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<LightSource>(Name);
        CopyTo(clone.get());
        return clone;
    }

    virtual void CopyTo(GameObject* target) const override {
        GameObject::CopyTo(target);
        if (target->ClassName == "LightSource") {
            auto* light = static_cast<LightSource*>(target);
            light->Color = Color;
            light->Intensity = Intensity;
        }
    }
};
