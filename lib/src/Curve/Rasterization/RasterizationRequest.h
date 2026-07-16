#pragma once

#include <Curve/Mesh/Vertex.h>
#include "Policies/Core/PointScalingPolicy.h"
#include "../../Inter/Dimensions.h"
#include "../../Obj/MorphPosition.h"

namespace Rasterization {
    struct RasterizationRequest {
        Dimensions dims;
        MorphPosition morph;

        PointScalingMode scalingMode { PointScalingMode::Unipolar };
        bool calcDepthDimensions { true };
        bool calcInterceptsOnly {};
        bool cyclic { true };
        bool decoupleComponentDeforms {};
        bool integralSampling {};
        bool interpolateCurves {};
        bool lowResCurves {};
        bool overrideDimension {};

        int noiseSeed { -1 };
        int overridingDimension { Vertex::Time };
        int primaryViewDimension { Vertex::Time };
        int paddingSize { 2 };

        float interceptPadding {};
        float xMinimum {};
        float xMaximum { 1.f };
    };

    inline PointScalingMode pointScalingModeFromLegacy(int scalingType) {
        return PointScalingPolicy::fromLegacyScalingType(scalingType).getMode();
    }

    inline PointScalingMode pointScalingModeFromLegacyFx(int scalingType) {
        return PointScalingPolicy::fromLegacyFxScalingType(scalingType).getMode();
    }
}
