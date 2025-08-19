#pragma once
#include "../Curve/CurvePiece.h"
#include "../Rasterizer/RasterizerParams.h"
#include "SamplerOutput.h"

using std::vector;

/**
 * ## Why
 *
 * We need to generate a waveform with equally-spaced samples.
 *
 * ## What
 *
 * @see @class SimpleCurveSampler
 * @see @class SamplerOutput
 */
class CurveSampler {
public:
    virtual ~CurveSampler() = default;

    virtual SamplerOutput buildFromCurves(vector<CurvePiece>& pieces, const SamplingParameters& params) = 0;
    virtual void sampleToBuffer(Buffer<float>& buffer, double delta) = 0;
    virtual float sampleAt(double position) = 0;

    [[nodiscard]] virtual bool isSampleable() const = 0;

private:
};

