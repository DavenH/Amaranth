#pragma once

#include <vector>

#include "../../RasterizerRuntime.h"
#include "../../WaveformBuffers.h"
#include "../../../Curve.h"
#include "../../../Intercept.h"
#include "../../../RasterizerData.h"
#include "../../../../Obj/ColorPoint.h"

namespace Rasterization {
    struct RasterizerSnapshotSource {
        const std::vector<Intercept>* intercepts {};
        const std::vector<ColorPoint>* colorPoints {};
        const std::vector<Curve>* curves {};
        WaveformBuffers waveform;
    };

    inline RasterizerSnapshotSource createRasterizerSnapshotSource(RasterizerRuntime runtime) {
        RasterizerSnapshotSource source;
        source.intercepts = runtime.intercepts;
        source.colorPoints = runtime.colorPoints;
        source.curves = runtime.curves;
        source.waveform = runtime.waveform.view();

        return source;
    }

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

            int size = source.waveform.waveX.size();

            if (size > 0) {
                target.buffer.ensureSize(size * 2);
                target.waveX = target.buffer.place(size);
                target.waveY = target.buffer.place(size);
                target.oneIndex = source.waveform.oneIndex;
                target.zeroIndex = source.waveform.zeroIndex;

                source.waveform.waveX.copyTo(target.waveX);
                source.waveform.waveY.copyTo(target.waveY);
            } else {
                target.waveX.nullify();
                target.waveY.nullify();
                target.oneIndex = 0;
                target.zeroIndex = 0;
            }
        }
    };
}
