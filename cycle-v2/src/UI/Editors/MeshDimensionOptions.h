#pragma once

#include <Curve/Mesh/MorphDimensions.h>

#include "UI/Editors/PropertySegmentedSelector.h"

namespace CycleV2 {

std::vector<PropertySegmentOption> meshDimensionOptions(
        MorphDimensions::Plane plane = MorphDimensions::Plane::YellowRed);
juce::String meshDimensionTooltip(int current, int target,
        MorphDimensions::Plane plane = MorphDimensions::Plane::YellowRed);

}
