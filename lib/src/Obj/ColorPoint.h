#pragma once

#include "Color.h"
#include "../Curve/Vertex2.h"

class ColorPoint {
public:
    ColorPoint(VertCube* cube, const Vertex2& a, const Vertex2& b, const Vertex2& c, int num) :
            cube(cube), before(a), mid(b), after(c), num(num) {
    }

    int num;
    Vertex2 before, mid, after;
    VertCube* cube;
private:
    JUCE_LEAK_DETECTOR(ColorPoint)
};
