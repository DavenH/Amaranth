#include "Geometry.h"
#include "NumberUtils.h"
#include "../Curve/Vertex2.h"

Vertex2 Geometry::getCrossPoint(
        float x1, float x2, float x3, float x4,
        float y1, float y2, float y3, float y4) {
    if((x1 == x3 && y1 == y3) ||
       (x1 == x4 && y1 == y4) ||
       (x2 == x3 && y2 == y3) ||
       (x2 == x4 && y2 == y4)) {
//      cout << "common points!!" << "\n";
        return {-1, -1};
    }

    float d = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

    // parallel
    if(d == 0) {
        if (NumberUtils::withinExclusive(x3, x1, x2) || NumberUtils::withinExclusive(x3, x2, x1)) {
            return {x3, y3};
        }

        if (NumberUtils::withinExclusive(x4, x1, x2) || NumberUtils::withinExclusive(x4, x2, x1)) {
            return {x4, y4};
        }
        return {-1, -1};
    }
    float r1 = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / d;
    float r2 = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / d;

    if(!NumberUtils::within(r1, 0.f, 1.f) || !NumberUtils::within(r2, 0.f, 1.f))
        return {-1, -1};

    return {x1 + r1 * (x2 - x1), y1 + r1 * (y2 - y1)};
}

bool Geometry::isOnSegment(double xi, double yi, double xj, double yj, double xk, double yk) {
    return (xi <= xk || xj <= xk) && (xk <= xi || xk <= xj) && (yi <= yk || yj <= yk)
            && (yk <= yi || yk <= yj);
}

char Geometry::computeDirection(double xi, double yi, double xj, double yj, double xk, double yk) {
    double a = (xk - xi) * (yj - yi);
    double b = (xj - xi) * (yk - yi);
    return a < b ? -1 : a > b ? 1 : 0;
}

/** Do line segments (x1, y1)--(x2, y2) and (x3, y3)--(x4, y4) intersect? */
int Geometry::doLineSegmentsIntersect(double x1, double x2, double x3, double x4,
                                      double y1, double y2, double y3, double y4) {
    // merely connected at ends
    if(x1 == x4 && y1 == y4 || x1 == x3 && y1 == y3 || x2 == x4 && y2 == y4 || x2 == x3 && y2 == y3)
        return IntersectAtEnds;

    char d1 = computeDirection(x3, y3, x4, y4, x1, y1);
    char d2 = computeDirection(x3, y3, x4, y4, x2, y2);
    char d3 = computeDirection(x1, y1, x2, y2, x3, y3);
    char d4 = computeDirection(x1, y1, x2, y2, x4, y4);

    return ((((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
            || (d1 == 0 && isOnSegment(x3, y3, x4, y4, x1, y1))
            || (d2 == 0 && isOnSegment(x3, y3, x4, y4, x2, y2))
            || (d3 == 0 && isOnSegment(x1, y1, x2, y2, x3, y3))
            || (d4 == 0 && isOnSegment(x1, y1, x2, y2, x4, y4))) ? Intersect : DoNotIntersect;
}

bool Geometry::withinBoundingBox(Vertex2& v, Vertex2& one, Vertex2& two) {
    return ((v.x >= one.x && v.x < two.x) ||
            ((v.x < one.x && v.x >= two.x) && (v.y >= one.y && v.y < two.y)) ||
            (v.y < one.y && v.y >= two.y));
}

Vertex2 Geometry::getSpacedVertex(int dist, Vertex2& a, Vertex2& b) {
    Vertex2 diff = b - a;
    float factor = dist / sqrtf(a.dist2(b));    // dist / sqrtf(a.x * a.x + b.x * b.x + a.y * a.y + b.y * b.y - 2 * (a.x * b.x + a.y * b.y));
    return diff * (1 + factor) + a;         // b * factor + a * (1 - factor);
}
