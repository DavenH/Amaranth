#pragma once

#include <Array/Buffer.h>

#include "Runtime/SpectralMagnitudeTransfer.h"

namespace CycleV2 {

enum class PortDomain;

enum class BinarySignalOperation {
    Add,
    Multiply
};

class BinarySignalMath final {
public:
    static bool applyTransfer(
            Buffer<float> values,
            const SpectralMagnitudeTransfer* transfer,
            size_t channel,
            int harmonicCount);
    static void combine(
            Buffer<float> output,
            Buffer<float> right,
            BinarySignalOperation operation);
    static void clampOutputDomain(Buffer<float> output, PortDomain domain);
};

}
