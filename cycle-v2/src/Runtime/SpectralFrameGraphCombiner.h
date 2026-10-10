#pragma once

#include <Audio/CycleDsp/SpectralStageCapture.h>

#include "Runtime/BinarySignalMath.h"

namespace CycleV2 {

struct SpectralFrameOperand {
    Buffer<float> values;
    const SpectralMagnitudeTransfer* transfer {};
    bool connected {};
};

class SpectralFrameGraphCombiner final {
public:
    SpectralFrameGraphCombiner(
            const CycleDsp::SpectralFrameCapture& capture,
            int activeHarmonicCount);

    static void applyPan(
            PortDomain domain,
            Buffer<float> source,
            Buffer<float> secondarySource,
            Buffer<float> left,
            Buffer<float> right,
            float pan,
            bool multiplicative);
    void add(
            SpectralFrameOperand left,
            SpectralFrameOperand right,
            Buffer<float> output,
            Buffer<float> scratch,
            PortDomain outputDomain,
            int channel) const;
    void multiply(
            SpectralFrameOperand left,
            SpectralFrameOperand right,
            Buffer<float> output,
            Buffer<float> scratch,
            PortDomain outputDomain,
            int channel) const;

private:
    void copyOperand(
            Buffer<float> output,
            SpectralFrameOperand operand,
            int channel) const;

    const CycleDsp::SpectralFrameCapture& capture;
    int activeHarmonicCount;
};

}
