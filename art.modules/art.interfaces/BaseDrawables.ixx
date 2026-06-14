module;
#include <SDL3/SDL.h>

export module BaseDrawables;

import Position2D;
import Size2D;

export class BaseDrawables
{
public:
    Position2D Position;
    Size2D Size;

    BaseDrawables(Position2D position, Size2D size) : Position(position), Size(size) {
    }
    virtual ~BaseDrawables() {}

    virtual void Draw(SDL_Renderer* renderer) = 0;
};