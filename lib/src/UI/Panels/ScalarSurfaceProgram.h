#pragma once

#include "ScalarSurfaceMaterial.h"

namespace ScalarSurfaceProgram {
bool isProgram(const ScalarSurfaceMaterial& material);
ScalarSurfaceHeightScales render(
        const std::vector<float>& unitValues,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material);
juce::Colour colourAt(const ScalarSurfaceHeightScales& product, int index);
}
