#pragma once
#include "../../Array/ScopedAlloc.h"

#include <vector>
using std::vector;

class TrilinearEdge;
struct Intercept;
class CurvePiece;

struct RasterizerData {
    RasterizerData() :
            zeroIndex(0), oneIndex(0) {
    }

    int zeroIndex, oneIndex;
    ScopedAlloc<float> buffer;
    Buffer<float> waveX, waveY;

    vector<TrilinearEdge> colorPoints;
    vector<Intercept> intercepts;
    vector<CurvePiece> curves;

    CriticalSection lock;
};
