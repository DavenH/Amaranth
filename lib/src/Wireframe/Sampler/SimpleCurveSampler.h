#pragma once

#include "CurveSampler.h"
#include "../Curve/CurvePiece.h"
#include "../../Array/ScopedAlloc.h"

class SimpleCurveSampler : public CurveSampler {
public:
    std::vector<CurvePiece> produceCurvePieces(const std::vector<Intercept>& controlPoints) override;
    void cleanUp();

private:
    void placeWaveBuffers(int size);
    Buffer<float> waveX, waveY, diffX, slope, area;

    ScopedAlloc<float> memoryBuffer;
};
