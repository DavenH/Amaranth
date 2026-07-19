#pragma once

#include <cstddef>
#include <vector>

namespace CycleV2::SpectralPreviewMapping {

std::vector<float> magnitudeSurface(
        const std::vector<float>& source,
        size_t columns,
        size_t rows);

std::vector<float> phaseSurface(
        const std::vector<float>& source,
        size_t columns,
        size_t rows);

}
