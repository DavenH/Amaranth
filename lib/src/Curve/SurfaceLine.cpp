#include "SurfaceLine.h"
#include "Vertex2.h"
#include "../Util/Geometry.h"
#include "../Util/Arithmetic.h"

SurfaceLine::SurfaceLine(Vertex* v1, Vertex* v2, int axis) :
    one(v1), two(v2), varyingAxis(axis), product(1) {
}

float SurfaceLine::at(float x) {
    return at(Vertex::Time, x);
}

float SurfaceLine::atProgress(int dim, float percent) {
    float xMax = jmax(one->values[dim], two->values[dim]);
    float xMin = jmin(one->values[dim], two->values[dim]);
    return at(dim, (xMax - xMin) * percent + xMin);
}

float SurfaceLine::at(int dim, float x) {
    return one->values[dim]
            + (x - one->values[varyingAxis]) * (two->values[dim] - one->values[dim])
                / (two->values[varyingAxis] - one->values[varyingAxis]);
}

float SurfaceLine::yAt(float y) {
    return yAt(Vertex::Phase, y);
}

float SurfaceLine::yAt(int dim, float y) {
    return one->values[varyingAxis]
            + (y - one->values[dim]) * (two->values[varyingAxis] - one->values[varyingAxis])
                / (two->values[dim] - one->values[dim]);
}

bool SurfaceLine::overlaps(float x, int dim) const {
    if(one->values[dim] < two->values[dim])
        return one->values[dim] <= x && two->values[dim] >= x;
    else
        return two->values[dim] <= x && one->values[dim] >= x;
}


bool SurfaceLine::owns(Vertex* vertex) {
    return one == vertex || two == vertex;
}


bool SurfaceLine::isAscending() {
    return (*one)[Vertex::Phase] < (*two)[Vertex::Phase];
}

bool SurfaceLine::operator==(const SurfaceLine& other) {
    bool ret = ((one == other.one && two == other.two)
                || (one == other.two && two == other.one));

    return ret;
}

bool SurfaceLine::operator<(const SurfaceLine& other) const {
    return one->values[Vertex::Phase] < other.one->values[Vertex::Phase];
}


Vertex2 SurfaceLine::getCrossPoint(Vertex* start, Vertex* end, int xDim, int yDim) const {
    return getCrossPoint(Vertex2(start->values[xDim], start->values[yDim]),
                         Vertex2(end->values[xDim], end->values[yDim]), xDim, yDim);
}


Vertex2 SurfaceLine::getCrossPoint(const Vertex2 start, const Vertex2 end, int xDim, int yDim) const {
    return Geometry::getCrossPoint(
            (*one)[xDim], (*two)[xDim], start.x, end.x, (*one)[yDim], (*two)[yDim], start.y, end.y);
}

void SurfaceLine::operator=(const SurfaceLine& other) {
    this->one = other.one;
    this->two = other.two;
}

Vertex* SurfaceLine::getOtherVertex(Vertex* v) {
    return (v == one) ? two : one;
}

Vertex* SurfaceLine::getMinAlongAxis(int dim) {
    return one->values[dim] < two->values[dim] ? one : two;
}

bool SurfaceLine::containsVertex(Vertex* vert) {
    return one == vert || two == vert;
}
