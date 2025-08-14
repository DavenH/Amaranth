#pragma once

class Vertex;

class DepthVert {
public:
    float x, y, alpha;
    Vertex* vert;

    DepthVert()
            : x(0), y(0), vert(nullptr), alpha(1) {
    }
    DepthVert(float x, float y, Vertex* vert, float alpha = 1)
            : x(x), y(y), vert(vert), alpha(alpha) {
    }

    bool operator!=(const DepthVert& other) {
        return !this->operator==(other);
    }

    bool operator==(const DepthVert& other) {
        return x == other.x && y == other.y && vert == other.vert;
    }
};
