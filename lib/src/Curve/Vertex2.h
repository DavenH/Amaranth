#pragma once

#include <cmath>
#include "SimpleIcpt.h"

class Vertex2 {
private:
    JUCE_LEAK_DETECTOR(Vertex2);
public:
    float x;
    float y;

    Vertex2(float x, float y) {
        this->x = x;
        this->y = y;
    }

    Vertex2() {
        x = y = 0;
    }

    Vertex2(const Vertex2& other) {
        x = other.x;
        y = other.y;
    }

    explicit Vertex2(const SimpleIcpt& icpt) : x(icpt.x), y(icpt.y) {
    }

    float dot(const Vertex2& v) const {
        return x * v.x + y * v.y;
    }

    float cross(const Vertex2& v) const {
        return x * v.y - y * v.x;
    }

    void constrainToUnit() {
        x = jlimit(0.f, 1.f, x);
        y = jlimit(0.f, 1.f, y);
    }

    void normalize() {
        float factor = 1 / sqrtf(x * x + y * y);
        x *= factor;
        y *= factor;
    }

    Vertex2 operator-(const Vertex2& other) const {
        return {x - other.x, y - other.y};
    }

    Vertex2 operator+(const Vertex2& other) const {
        return {x + other.x, y + other.y};
    }

    Vertex2 operator*(const float factor) const {
        return {x * factor, y * factor};
    }

    Vertex2 operator*(const Vertex2& mult) const {
        return {x * mult.x, y * mult.y};
    }

    float operator[](const int index) const {
        return (index == 0) ? x : y;
    }

    bool operator<(const Vertex2& comp) const {
        return x < comp.x;
    }

    Vertex2& operator+=(const Vertex2& other) {
        x += other.x;
        y += other.y;

        return *this;
    }

    bool operator==(const Vertex2& comp) const {
        return x == comp.x && y == comp.y;
    }

    bool operator!=(const Vertex2& comp) const {
        return !(*this == comp);
    }

    bool isSmallerThan(float thresh) const {
        return jmax(fabsf(x), fabsf(y)) < thresh;
    }

    float angleTo(const Vertex2& other) const {
        return atan2f(other.y, other.x) - atan2f(y, x);
    }

    float dist2(const Vertex2& other) const {
        return (x - other.x) * (x - other.x) + (y - other.y) * (y - other.y);
    }

    bool isntNull() const {
        return x != 0.f || y != 0.f;
    }
};
