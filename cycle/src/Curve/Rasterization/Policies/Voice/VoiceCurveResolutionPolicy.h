#pragma once

#include <vector>

#include <Curve/Curve.h>
#include <Curve/Rasterization/Policies/Curves/CurveResolutionPolicy.h>

namespace Cycle::Rasterization {
    class VoiceCurveResolutionPolicy {
    public:
        void apply(std::vector<Curve>& curves) const {
            if (curves.size() < 2) {
                return;
            }

            ::Rasterization::CurveResolutionPolicy::applyResolutionIndices(
                    curves,
                    0.05f / float(Curve::resolution),
                    1);

            int lastIdx = (int) curves.size() - 1;
            curves[0].resIndex = curves[1].resIndex;
            curves[lastIdx].resIndex = curves[lastIdx - 1].resIndex;
        }
    };
}
