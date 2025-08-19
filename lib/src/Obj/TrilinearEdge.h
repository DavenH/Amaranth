#pragma once

#include "Color.h"
#include "../Wireframe/Vertex/Vertex2.h"

class TrilinearEdge {
public:
    TrilinearEdge(TrilinearCube* cube, const Vertex2& a, const Vertex2& b, const Vertex2& c, int num) :
            cube(cube), before(a), mid(b), after(c), num(num) {
    }

    int num;
    Vertex2 before, mid, after;
    TrilinearCube* cube;
private:
    JUCE_LEAK_DETECTOR(TrilinearEdge)
};
