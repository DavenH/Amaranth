#pragma once
#include "../Curve/Vertex2.h"

struct ColorPos {
    void update(const Vertex2& v, const Color& c) {
        x = v.x;
        y = v.y;
        this->c = c;
    }

    void update(float x, float y, const Color& c) {
        this->x = x;
        this->y = y;
        this->c = c;
    }

    float x, y;
    Color c;
};