#pragma once

#include <iostream>
#include "Interpolator/Trilinear/TrilinearVertex.h"
#include "../Util/Arithmetic.h"

class Vertex2;

class SurfaceLine {
    JUCE_LEAK_DETECTOR(SurfaceLine)

public:
    int varyingAxis;
    float product;

    SurfaceLine(Vertex* v1, Vertex* v2, int axis = Vertex::Time); //bool isSplit = false
    ~SurfaceLine() = default;

    float at(float x);
    float at(int dim, float x);
    float yAt(float y);
    float yAt(int dim, float y);
    float atProgress(int dim, float x);

    [[nodiscard]] bool overlaps(float x, int dim) const;
    bool isAscending();
    bool owns(Vertex* vertex);
    bool operator==(const SurfaceLine& other);
    bool operator<(const SurfaceLine& other) const;
    void operator=(const SurfaceLine& other);
    Vertex2 getCrossPoint(Vertex* start, Vertex* end, int dimA, int dimB) const;
    [[nodiscard]] Vertex2 getCrossPoint(Vertex2 start, Vertex2 end, int xDim, int yDim) const;
    Vertex* getOtherVertex(Vertex* v);
    Vertex* getMinAlongAxis(int dim);
    bool containsVertex(Vertex* vert);

    Vertex* one;
    Vertex* two;
};