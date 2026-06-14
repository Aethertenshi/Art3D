module;
#include <string>
#include <vector>
#include <memory>

export module GameObject;

import Position2D;
import Size2D;

export class GameObject
{
public:
    std::string Name;
    Position2D Position;
    Size2D Size;
    std::vector<std::shared_ptr<GameObject>> Children;
    GameObject* Parent = nullptr;

    GameObject(const std::string& name, Position2D position, Size2D size)
        : Name(name), Position(position), Size(size) {}

    void AddChild(std::shared_ptr<GameObject> child) {
        child->Parent = this;
        Children.push_back(child);
    }
};
