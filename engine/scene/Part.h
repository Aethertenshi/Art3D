#pragma once
#include <string>
#include "core/Vector3D.h"
#include "scene/BasePart.h"

class Part : public BasePart
{
public:
    Vector3D Color;

    Part(const std::string& name)
        : BasePart(name), Color(1.0f, 1.0f, 1.0f) {
        ClassName = "Part";
    }

    std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<Part>(Name);
        CopyTo(clone.get());
        return clone;
    }

    void CopyTo(GameObject* target) const override {
        BasePart::CopyTo(target);
        if (target->ClassName == "Part") {
            static_cast<Part*>(target)->Color = Color;
        }
    }
};
