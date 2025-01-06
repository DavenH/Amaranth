#pragma once

#include "Buffer.h"
#include "VecOps.h"
#include "../Curve/Vertex2.h"

class BufferXY {
public:
    Buffer<float> x;
    Buffer<float> y;

    Vertex2 front() {
        if (x.empty()) {
            return {};
        }

        return {x.front(), y.front()};
    }

    Vertex2 back() {
        if (x.empty()) {
            return {};
        }

        return {x.back(), y.back()};
    }

    void zero() {
        x.zero();
        y.zero();
    }

    Vertex2 operator[](const int index) {
        if (index >= x.size())
            return {};

        return {x[index], y[index]};
    }

    template<class PointType>
    void set(int index, const PointType& point) {
        x[index] = point.x;
        y[index] = point.y;
    }

    void set(int index, float ex, float why) {
        x[index] = ex;
        y[index] = why;
    }

    int size() const {
        jassert(x.size() == y.size());
        return x.size();
    }

    bool empty() const {
        return x.empty();
    }

    void interleaveTo(const Buffer<float>& buffer) const {
        VecOps::interleave(x, y, buffer);
    }

    void copyFrom(const Buffer<float>& ex, const Buffer<float>& why) const {
        ex.copyTo(x);
        why.copyTo(y);
    }
};
