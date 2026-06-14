module;
#include <SDL3/SDL.h>

export module Frame;

import Position2D;
import Size2D;
import BaseDrawables;

export class Frame : public BaseDrawables
{
public:
    Frame(Position2D position, Size2D size) : BaseDrawables(position, size) {
    }

    void Draw(SDL_Renderer* renderer) override {
        SDL_FRect rect = {
            (float)Position.OffsetX,
            (float)Position.OffsetY,
            (float)Size.OffsetX,
            (float)Size.OffsetY
        };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Draw frame in white
        SDL_RenderRect(renderer, &rect);
    }
};