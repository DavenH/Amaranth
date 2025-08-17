#pragma once
#include "../Curve/CurvePiece.h"
#include "Wireframe/State/RasterizerData.h"

using std::vector;

/**
 * ## Why
 *
 * We need to generate a waveform with equally-spaced samples.
 *
 * ## What
 *
 * @see @class SimpleCurveSampler
 */
class CurveSampler {
public:
    virtual ~CurveSampler() = default;

    virtual void sampleToBuffer(Buffer<float>& buffer, double delta) = 0;

    [[nodiscard]] virtual bool isSampleable() const = 0;
    virtual float sampleAt(double position) = 0;

private:
};

