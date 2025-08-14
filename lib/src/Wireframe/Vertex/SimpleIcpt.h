#pragma once
class TrilinearCube;

class SimpleIcpt {
public:
    bool isWrapped;
    bool pole; // ?
    float x, y;
    TrilinearCube* parent;

    SimpleIcpt()
        : isWrapped(false), pole(false), x(0), y(0), parent(0) {
    }

    SimpleIcpt(float x, float y, TrilinearCube* cube, bool pole, bool isWrapped = false)
            : x(x), y(y), parent(cube), pole(pole), isWrapped(isWrapped) {
    }

    bool operator!=(const SimpleIcpt& other) {
        return !this->operator==(other);
    }

    bool operator==(const SimpleIcpt& other) const {
        return x == other.x && y == other.y && parent == other.parent;
    }
};

