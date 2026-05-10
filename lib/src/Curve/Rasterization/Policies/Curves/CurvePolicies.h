#pragma once

#include <vector>

#include "../../../Curve.h"
#include "../../../Intercept.h"

namespace Rasterization {
    class CurveTripletPolicy {
    public:
        static void appendAdjacent(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves) {
            for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
                curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
            }
        }

        static void appendAdjacentReversed(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves) {
            for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
                int idx = (int) intercepts.size() - 1 - i;
                curves.emplace_back(intercepts[idx], intercepts[idx - 1], intercepts[idx - 2]);
            }
        }
    };

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

            for (auto& curve : curves) {
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

    class CurveWaveformPreparationPolicy {
    public:
        void apply(std::vector<Curve>& curves) const {
            preserveComponentGuideContinuity(curves);

            for (auto& curve : curves) {
                curve.recalculateCurve();
            }
        }

    private:
        static void preserveComponentGuideContinuity(std::vector<Curve>& curves) {
            for (int i = 0; i < (int) curves.size(); ++i) {
                Curve& curve = curves[i];

                if (i >= (int) curves.size() - 1 || curve.b.cube == nullptr) {
                    continue;
                }

                if (curve.b.cube->getCompGuideCurve() < 0) {
                    continue;
                }

                Curve& next = curves[i + 1];

                if (next.b.cube != nullptr && next.b.cube->getCompGuideCurve() >= 0) {
                    continue;
                }

                curve.c.shp = 1.f;
                next.b.shp = 1.f;
                next.updateCurrentIndex();

                if (i < (int) curves.size() - 2) {
                    curves[i + 2].a.shp = 1.f;
                }
            }
        }
    };

    class InterceptPaddingFlagPolicy {
    public:
        void apply(std::vector<Intercept>& intercepts) const {
            bool padAny = false;

            for (int i = 0; i < (int) intercepts.size() - 1; ++i) {
                Intercept& current = intercepts[i];
                Intercept& next = intercepts[i + 1];
                bool pad = current.cube != nullptr && current.cube->getCompGuideCurve() >= 0;
                current.padBefore = pad;
                next.padAfter = pad;

                padAny |= pad;
            }

            if (! padAny) {
                return;
            }

            for (int i = 1; i < (int) intercepts.size(); ++i) {
                intercepts[i].padBefore &= ! intercepts[i - 1].padBefore;
            }

            for (int i = 0; i < (int) intercepts.size() - 1; ++i) {
                intercepts[i].padAfter &= ! intercepts[i + 1].padAfter;
            }
        }
    };
}
