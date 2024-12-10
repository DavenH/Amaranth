#pragma once
#include "../Util/NumberUtils.h"
#include "../Curve/Vertex2.h"

[[nodiscard]]
class CurveLine {
public:
    CurveLine() : bounded(false), nan(false), one(0, 0), two(1, 0), slope(0) {
    }

    CurveLine(const Vertex2& a, const Vertex2& b) : bounded(true), nan(false), one(a), two(b) {
        if (two.x == one.x) {
            nan = true;
            slope = 0;
        } else {
            slope = (two.y - one.y) / (two.x - one.x);
        }
    }

    CurveLine(const Vertex2& point, float slope) : bounded(false), nan(false), one(point) {
        this->slope = slope;
        two = Vertex2(one.x + 1, at(one.x + 1));
    }

    bool isBounded() const {
        return bounded;
    }

    float at(float x) const {
        jassert(!nan);
        return one.y + (x - one.x) * slope;
    }

    float rootOf(float y) const {
        if (slope == 0.f)
            return 10000.f;
        return one.x + (y - one.y) / slope;
    }

    Vertex2 getIntercept(const CurveLine& line) const {
        float x = (one.y - line.one.y) / (slope - line.slope);
        return {x, at(x)};
    }

    bool isColinearTo(const CurveLine& line) const {
        return fabsf(slope - line.slope) < 0.00001f;
    }

    static bool arePointsColinear(const Vertex2& a, const Vertex2& b, const Vertex2& c) {
        CurveLine first(a, b);
        CurveLine second(b, c);

        return first.isColinearTo(second);
    }

    void setPoints(const Vertex2& a, const Vertex2& b) {
        one = a;
        two = b;
        nan = two.x == one.x;
        slope = 0.f;
        if (!nan) {
            slope = (two.y - one.y) / (two.x - one.x);
        }

        bounded = true;
    }

    float distanceToPoint(const Vertex2& point) const {
        float x2 = (point.x - one.x) * (two.x - one.x);
        float y2 = (point.y - one.y) * (two.y - one.y);
        float u = (x2 + y2) / one.dist2(two);

        if (NumberUtils::within(u, 0.f, 1.f)) {
            Vertex2 intercept = one * (1 - u) + two * u;
            return sqrtf(intercept.dist2(point));
        }

        return -1.f;
    }

    CurveLine& operator=(const CurveLine& other) {
        one = other.one;
        two = other.two;

        slope = other.slope;
        bounded = other.bounded;

        return *this;
    }

    CurveLine operator*(float factor) const {
        return {one * factor, slope * factor};
    }

    CurveLine operator+(const CurveLine& right) const {
        return {one + right.one, slope + right.slope};
    }

    Vertex2 one;
    Vertex2 two;
    float slope;

private:
    bool bounded;
    bool nan;

    JUCE_LEAK_DETECTOR(CurveLine)
};
