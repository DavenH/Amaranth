#pragma once

#include <cstddef>
#include <vector>

namespace CycleV2 {

class PreviewContrastNormalization {
public:
    static void apply(std::vector<float>& values, float targetPeak);
    static void applySpectralMagnitude(
            std::vector<float>& values,
            size_t columns,
            size_t rows,
            float targetPeak);
};

}
