#pragma once

struct Size2D
{
public:
    int ScaleX;
    int ScaleY;
    int OffsetX;
    int OffsetY;

    Size2D(int scaleX, int scaleY, int offsetX, int offsetY){
        ScaleX = scaleX;
        ScaleY = scaleY;
        OffsetX = offsetX;
        OffsetY = offsetY;
    }
    ~Size2D(){
        
    }

    Size2D FromScale(int scaleX, int scaleY){
        return Size2D(scaleX, scaleY, 0, 0);
    }
    Size2D FromOffset(int offsetX, int offsetY){
        return Size2D(offsetX, offsetY, 0, 0);
    }
};
