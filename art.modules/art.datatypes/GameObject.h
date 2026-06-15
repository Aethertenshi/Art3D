#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Position2D.h"
#include "Size2D.h"
#include "Vector3D.h"

class GameObject
{
public:
    std::string Name;
    std::string ClassName = "GameObject";
    Position2D Position;
    Size2D Size;
    std::vector<std::shared_ptr<GameObject>> Children;
    GameObject* Parent = nullptr;
    SDL_GPUDevice* GpuDevice = nullptr;

    bool Is3D = false;
    Vector3D Position3D = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D Rotation3D = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D Scale3D = Vector3D(1.0f, 1.0f, 1.0f);
    Vector3D PositionOffset = Vector3D(0.0f, 0.0f, 0.0f);
    Vector3D RotationOffset = Vector3D(0.0f, 0.0f, 0.0f);

    // Custom mesh fields (for OBJ importing)
    std::shared_ptr<SDL_GPUBuffer> CustomVertexBuffer = nullptr;
    std::shared_ptr<SDL_GPUBuffer> CustomIndexBuffer = nullptr;
    uint32_t CustomIndexCount = 0;

    GameObject(const std::string& name, Position2D position, Size2D size)
        : Name(name), ClassName("GameObject"), Position(position), Size(size), Is3D(false) {}

    GameObject(const std::string& name, Vector3D position, Vector3D rotation, Vector3D scale)
        : Name(name), ClassName("GameObject"), Position(0, 0, 0, 0), Size(0, 0, 0, 0), Is3D(true),
          Position3D(position), Rotation3D(rotation), Scale3D(scale) {}

    void AddChild(std::shared_ptr<GameObject> child) {
        child->Parent = this;
        child->GpuDevice = GpuDevice;
        Children.push_back(child);
    }

    virtual std::shared_ptr<GameObject> Clone() const {
        std::shared_ptr<GameObject> clone;
        if (Is3D) {
            clone = std::make_shared<GameObject>(Name, Position3D, Rotation3D, Scale3D);
        } else {
            clone = std::make_shared<GameObject>(Name, Position, Size);
        }
        CopyTo(clone.get());
        return clone;
    }

    virtual void CopyTo(GameObject* target) const {
        target->Name = Name;
        target->ClassName = ClassName;
        target->Is3D = Is3D;
        target->Position = Position;
        target->Size = Size;
        target->Position3D = Position3D;
        target->Rotation3D = Rotation3D;
        target->Scale3D = Scale3D;
        target->PositionOffset = PositionOffset;
        target->RotationOffset = RotationOffset;
        target->CustomVertexBuffer = CustomVertexBuffer;
        target->CustomIndexBuffer = CustomIndexBuffer;
        target->CustomIndexCount = CustomIndexCount;
        target->GpuDevice = GpuDevice;
        target->Children.clear();
        for (const auto& child : Children) {
            target->AddChild(child->Clone());
        }
    }

    Vector3D GetWorldPosition() const {
        if (Parent && Is3D) {
            return Parent->GetWorldPosition() + Position3D;
        }
        return Position3D;
    }

    Vector3D GetWorldRotation() const {
        if (Parent && Is3D) {
            return Parent->GetWorldRotation() + Rotation3D;
        }
        return Rotation3D;
    }

    Vector3D GetWorldScale() const {
        if (Parent && Is3D) {
            Vector3D parentScale = Parent->GetWorldScale();
            return Vector3D(parentScale.X * Scale3D.X, parentScale.Y * Scale3D.Y, parentScale.Z * Scale3D.Z);
        }
        return Scale3D;
    }
};
