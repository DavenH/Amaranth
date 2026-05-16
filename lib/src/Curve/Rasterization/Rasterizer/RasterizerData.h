#pragma once

#include <vector>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/Intercept.h>
#include <Obj/ColorPoint.h>

using std::vector;

struct RasterizerData {
    RasterizerData() :
            zeroIndex(0), oneIndex(0) {
    }

    int zeroIndex, oneIndex;
    int paddingSize {};
    bool wrapsVertices {};
    ScopedAlloc<float> buffer;
    Buffer<float> waveX, waveY;

    vector<ColorPoint> colorPoints;
    vector<Intercept> intercepts;
    vector<Curve> curves;

    CriticalSection lock;
};
