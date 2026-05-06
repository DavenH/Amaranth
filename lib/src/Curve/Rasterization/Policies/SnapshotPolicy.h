#pragma once

#include <vector>

#include "../../Curve.h"
#include "../../Intercept.h"
#include "../../RasterizerData.h"
#include "../../../Array/Buffer.h"
#include "../../../Obj/ColorPoint.h"

namespace Rasterization {
    struct RasterizerSnapshotSource {
        const std::vector<Intercept>* intercepts {};
        const std::vector<ColorPoint>* colorPoints {};
        const std::vector<Curve>* curves {};
        Buffer<float> waveX;
        Buffer<float> waveY;
        int zeroIndex {};
        int oneIndex {};
    };

    class NoSnapshot {
    public:
        void publish(RasterizerData&, const RasterizerSnapshotSource&) const {
        }
    };

    class RasterizerDataSnapshot {
    public:
        void publish(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            ScopedLock sl(target.lock);

            if (source.intercepts != nullptr) {
                target.intercepts = *source.intercepts;
            } else {
                target.intercepts.clear();
            }

            if (source.colorPoints != nullptr) {
                target.colorPoints = *source.colorPoints;
            } else {
                target.colorPoints.clear();
            }

            if (source.curves != nullptr) {
                target.curves = *source.curves;
            } else {
                target.curves.clear();
            }

            int size = source.waveX.size();

            if (size > 0) {
                target.buffer.ensureSize(size * 2);
                target.waveX = target.buffer.place(size);
                target.waveY = target.buffer.place(size);
                target.oneIndex = source.oneIndex;
                target.zeroIndex = source.zeroIndex;

                source.waveX.copyTo(target.waveX);
                source.waveY.copyTo(target.waveY);
            } else {
                target.waveX.nullify();
                target.waveY.nullify();
                target.oneIndex = 0;
                target.zeroIndex = 0;
            }
        }
    };
}
