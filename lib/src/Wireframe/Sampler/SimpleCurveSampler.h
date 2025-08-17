#pragma once

#include "CurveSampler.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Util/Arithmetic.h"
#include "../Curve/CurvePiece.h"
#include "../State/RasterizerParameters.h"

class SimpleCurveSampler : public CurveSampler {
public:

    // separated into two steps because we may want to sample multiple times
    void buildFromCurves(
        const std::vector<CurvePiece>& curves,
        const SamplingParameters& params
    );

    // Sampling API
    float sampleAt(double position) override;
    void sampleToBuffer(Buffer<float>& buffer, double delta) override;
    [[nodiscard]] bool isSampleable() const override { return waveX.size() > 1; }

    // Accessors (useful in tests)
    [[nodiscard]] Buffer<float> getWaveX() const { return waveX; }
    [[nodiscard]] Buffer<float> getWaveY() const { return waveY; }
    [[nodiscard]] Buffer<float> getSlope() const { return slope; }
    [[nodiscard]] int getZeroIndex() const { return zeroIndex; }
    [[nodiscard]] int getOneIndex() const { return oneIndex; }

    void cleanUp();

private:
    static void ensureTransferTable();
    void placeWaveBuffers(int size);

    Buffer<float> waveX, waveY, diffX, slope, area;
    ScopedAlloc<float> memoryBuffer;

    int zeroIndex{0};
    int oneIndex{0};

    static float transferTable[CurvePiece::resolution];
};
