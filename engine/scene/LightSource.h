#pragma once
#include <string>
#include "core/GameObject.h"
#include "core/Vector3D.h"

enum class LightType {
    Point = 0,
    Spot = 1,
    Directional = 2
};

class LightSource : public GameObject
{
public:
    LightType Type = LightType::Point;
    Vector3D Color;
    float Intensity;
    Vector3D Direction;
    float SpotAngleInner;
    float SpotAngleOuter;
    float Range;
    bool CastShadow;

    LightSource(const std::string& name)
        : GameObject(name, Vector3D(0.0f, 3.0f, 0.0f), Vector3D(0.0f, 0.0f, 0.0f), Vector3D(1.0f, 1.0f, 1.0f)),
          Color(1.0f, 1.0f, 1.0f), Intensity(1.0f),
          Direction(0.0f, -1.0f, 0.0f),
          SpotAngleInner(20.0f), SpotAngleOuter(45.0f),
          Range(10.0f), CastShadow(true)
    {
        ClassName = "LightSource";
    }

    std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<LightSource>(Name);
        CopyTo(clone.get());
        return clone;
    }

    void CopyTo(GameObject* target) const override {
        GameObject::CopyTo(target);
        if (target->ClassName == "LightSource") {
            auto* light = static_cast<LightSource*>(target);
            light->Type = Type;
            light->Color = Color;
            light->Intensity = Intensity;
            light->Direction = Direction;
            light->SpotAngleInner = SpotAngleInner;
            light->SpotAngleOuter = SpotAngleOuter;
            light->Range = Range;
            light->CastShadow = CastShadow;
        }
    }
};
