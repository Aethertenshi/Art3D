#pragma once
#include <string>
#include "core/GameObject.h"

class LogicScript : public GameObject
{
public:
    std::string SourceCode;

    LogicScript(const std::string& name)
        : GameObject(name, Position2D(0, 0, 0, 0), Size2D(0, 0, 0, 0))
    {
        ClassName = "LogicScript";
        Is3D = false;
        SourceCode = "// Welcome to Artscript pre0!... \nprint('hello from artscript!')";
    }

    std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<LogicScript>(Name);
        CopyTo(clone.get());
        return clone;
    }

    void CopyTo(GameObject* target) const override {
        GameObject::CopyTo(target);
        if (target->ClassName == "LogicScript") {
            static_cast<LogicScript*>(target)->SourceCode = SourceCode;
        }
    }
};
