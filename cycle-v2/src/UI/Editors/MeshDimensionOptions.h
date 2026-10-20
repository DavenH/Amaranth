#pragma once

#include "UI/Editors/PropertySegmentedSelector.h"

namespace CycleV2 {

inline std::vector<PropertySegmentOption> meshDimensionOptions() {
    return {
            { "Static", "0", "meshDimensions.static", "One curve without morph axes" },
            { "1D", "1", "meshDimensions.one", "Morph along Yellow" },
            { "2D", "2", "meshDimensions.two", "Morph along Red and Blue" },
            { "3D", "3", "meshDimensions.three", "Morph along Yellow, Red and Blue" }
    };
}

}
