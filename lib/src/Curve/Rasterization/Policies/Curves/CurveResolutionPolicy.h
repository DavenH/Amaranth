#pragma once

#include <vector>

#include "../../../Curve.h"

namespace Rasterization {
    class CurveResolutionPolicy {
    public:
        struct Context {
            bool lowResCurves {};
            bool integralSampling {};
            bool interpolateCurves {};
            int paddingSize { 2 };
        };

        void apply(std::vector<Curve>& curves, const Context& context) const {
            if (curves.empty()) {
                return;
            }

            if (context.lowResCurves && curves.size() > 8) {
                for (auto& curve : curves) {
                    curve.resIndex = Curve::resolutions - 1;
                    curve.setShouldInterpolate(false);
                }

                return;
            }

            for(auto& curve : curves) {
                curve.setShouldInterpolate(! context.lowResCurves && context.interpolateCurves);
            }

            float baseFactor = context.lowResCurves ? 0.4f : context.integralSampling ? 0.05f : 0.1f;
            applyResolutionIndices(curves, baseFactor / float(Curve::resolution), context.paddingSize);
        }

        static void applyResolutionIndices(std::vector<Curve>& curves, float base, int paddingSize) {
            if (curves.size() < 2) {
                return;
            }

            int lastIdx = (int) curves.size() - 1;

            for (int i = 1; i < (int) curves.size() - 1; ++i) {
                float dx = curves[i + 1].c.x - curves[i - 1].a.x;

                for (int j = 0; j < Curve::resolutions; ++j) {
                    int res = Curve::resolution >> j;

                    if (dx < base * float(res)) {
                        curves[i].resIndex = j;
                    }
                }
            }

            int padding = paddingSize;
            curves.front().resIndex = curves[lastIdx - 2 * (padding - 1)].resIndex;
            curves.back().resIndex = curves[2 * padding - 1].resIndex;
        }
    };
}
