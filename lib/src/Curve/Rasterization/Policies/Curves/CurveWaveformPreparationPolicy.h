#pragma once

#include <vector>

#include "../Mesh/ComponentGuideSharpnessPolicy.h"
#include "../../../Curve.h"

namespace Rasterization {
    class CurveWaveformPreparationPolicy {
    public:
        void apply(std::vector<Curve>& curves) const {
            ComponentGuideSharpnessPolicy().apply(curves);

            for (auto& curve : curves) {
                curve.recalculateCurve();
            }
        }
    };
}
