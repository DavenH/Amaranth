#pragma once

#include <vector>

#include "../Array/ScopedAlloc.h"
#include "../Obj/ColorPoint.h"
#include "Curve.h"
#include "Intercept.h"

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
