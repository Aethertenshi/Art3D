#pragma once
#include <string>
#include "../art.datatypes/BasePart.h"
#include "../art.datatypes/Vector3D.h"

class Part : public BasePart
{
public:
    Vector3D Color; // Simple RGB color parameter for the 3D part representation
    
    Part(const std::string& name) 
        : BasePart(name), Color(1.0f, 1.0f, 1.0f) {
        ClassName = "Part";
    }

    virtual std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<Part>(Name);
        CopyTo(clone.get());
        return clone;
    }

    virtual void CopyTo(GameObject* target) const override {
        BasePart::CopyTo(target);
        if (target->ClassName == "Part") {
            static_cast<Part*>(target)->Color = Color;
        }
    }
};
