#pragma once
#include <vector>
#include "../../Array/ScopedAlloc.h"

using std::vector;

class ColorPoint;
struct Intercept;
class CurvePiece;

struct RasterizerData {
    RasterizerData() :
            zeroIndex(0), oneIndex(0) {
    }

    int zeroIndex, oneIndex;
    ScopedAlloc<float> buffer;
    Buffer<float> waveX, waveY;

    vector<ColorPoint> colorPoints;
    vector<Intercept> intercepts;
    vector<CurvePiece> curves;

    CriticalSection lock;
};
