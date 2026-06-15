#pragma once
#include <string>
#include "../art.datatypes/GameObject.h"
#include "../art.datatypes/Position2D.h"
#include "../art.datatypes/Size2D.h"

class BaseDrawables : public GameObject
{
public:
    BaseDrawables(const std::string& name, Position2D position, Size2D size)
        : GameObject(name, position, size) {
        Is3D = false;
        ClassName = "BaseDrawables";
    }
    virtual ~BaseDrawables() {}

    virtual void Draw() = 0;
};
