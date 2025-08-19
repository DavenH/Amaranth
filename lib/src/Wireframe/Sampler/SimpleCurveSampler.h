#pragma once

#include "CurveSampler.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Util/Arithmetic.h"
#include "../Curve/CurvePiece.h"
#include "../Rasterizer/RasterizerParams.h"

class SimpleCurveSampler : public CurveSampler {
public:

    SamplerOutput buildFromCurves(
        std::vector<CurvePiece>& pieces,
        const SamplingParameters& params
    ) override;

    // Sampling API
    float sampleAt(double position) override;
    void sampleToBuffer(Buffer<float>& buffer, double delta) override;
    [[nodiscard]] bool isSampleable() const override { return waveX.size() > 1; }

    // Accessors (useful in tests)
    [[nodiscard]] Buffer<float> getWaveX() const { return waveX; }
    [[nodiscard]] Buffer<float> getWaveY() const { return waveY; }
    [[nodiscard]] Buffer<float> getSlope() const { return slope; }
    [[nodiscard]] Buffer<float> getDiffX() const { return diffX; }
    [[nodiscard]] Buffer<float> getArea() const { return area; }
    [[nodiscard]] int getZeroIndex() const { return zeroIndex; }
    [[nodiscard]] int getOneIndex() const { return oneIndex; }

    void cleanUp();

protected:
    virtual int getCurveResolution(const CurvePiece& thisCurve, const CurvePiece& nextCurve) const;
    virtual void generateWaveformForCurve(int& waveIdx, const CurvePiece& thisCurve, const CurvePiece& nextCurve, int curveRes);

    void setZeroIndex(int index) { zeroIndex = index; }
    void setOneIndex(int index) { oneIndex = index; }

    static void ensureTransferTable();
    void placeWaveBuffers(int size);

    Buffer<float> waveX, waveY, diffX, slope, area;
    ScopedAlloc<float> memoryBuffer;

    int zeroIndex{0};
    int oneIndex{0};

    static float transferTable[CurvePiece::resolution];
};
