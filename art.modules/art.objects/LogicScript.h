#pragma once
#include <string>
#include "../art.datatypes/GameObject.h"

class LogicScript : public GameObject
{
public:
    std::string SourceCode;

    LogicScript(const std::string& name)
        : GameObject(name, Position2D(0, 0, 0, 0), Size2D(0, 0, 0, 0))
    {
        ClassName = "LogicScript";
        Is3D = false;
        SourceCode = "// Write your Wren code here...\n\nclass CustomScript {\n  construct new(gameObject) {\n    _gameObject = gameObject\n  }\n\n  update(deltaTime) {\n    // code...\n  }\n}\n";
    }

    virtual std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<LogicScript>(Name);
        CopyTo(clone.get());
        return clone;
    }

    virtual void CopyTo(GameObject* target) const override {
        GameObject::CopyTo(target);
        if (target->ClassName == "LogicScript") {
            static_cast<LogicScript*>(target)->SourceCode = SourceCode;
        }
    }
};
