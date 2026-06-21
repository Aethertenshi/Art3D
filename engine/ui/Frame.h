#pragma once
#include <SDL3/SDL.h>
#include "ui/BaseDrawables.h"
#include "imgui.h"

class Frame : public BaseDrawables
{
public:
    Frame(const std::string& name, Position2D position, Size2D size)
        : BaseDrawables(name, position, size) {
        ClassName = "Frame";
    }

    std::shared_ptr<GameObject> Clone() const override {
        auto clone = std::make_shared<Frame>(Name, Position, Size);
        CopyTo(clone.get());
        return clone;
    }

    void Draw() override {
        ImGui::GetForegroundDrawList()->AddRect(
            ImVec2((float)Position.OffsetX, (float)Position.OffsetY),
            ImVec2((float)(Position.OffsetX + Size.OffsetX), (float)(Position.OffsetY + Size.OffsetY)),
            IM_COL32(255, 255, 255, 255)
        );
    }
};
