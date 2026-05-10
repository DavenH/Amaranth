#pragma once

#include <vector>

#include "../../../Curve.h"

namespace Rasterization {
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
}
