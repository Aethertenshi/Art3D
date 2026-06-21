#pragma once

struct Position2D
{
    int ScaleX;
    int ScaleY;
    int OffsetX;
    int OffsetY;

    Position2D(int scaleX, int scaleY, int offsetX, int offsetY)
        : ScaleX(scaleX), ScaleY(scaleY), OffsetX(offsetX), OffsetY(offsetY) {}

    static Position2D FromScale(int scaleX, int scaleY) {
        return Position2D(scaleX, scaleY, 0, 0);
    }
    static Position2D FromOffset(int offsetX, int offsetY) {
        return Position2D(offsetX, offsetY, 0, 0);
    }
};
