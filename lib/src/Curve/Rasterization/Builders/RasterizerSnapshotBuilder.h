#pragma once

#include <vector>

#include <Curve/Curve.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Rasterization/Rasterizer/RasterizerData.h>
#include <Curve/Rasterization/WaveformBuffers.h>
#include <Obj/ColorPoint.h>

namespace Rasterization {
    struct RasterizerSnapshotSource {
        const std::vector<Intercept>* intercepts {};
        const std::vector<ColorPoint>* colorPoints {};
        const std::vector<Curve>* curves {};
        WaveformBuffers waveform;
        int paddingSize {};
        bool wrapsVertices {};
        bool sampleable {};
    };

    class RasterizerSnapshotBuilder {
    public:
        void publish(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            auto next = std::make_shared<RasterizerSnapshotData>();

            next->paddingSize = source.paddingSize;
            next->wrapsVertices = source.wrapsVertices;
            next->sampleable = source.sampleable;

            if (source.intercepts != nullptr) {
                next->intercepts = *source.intercepts;
            }

            if (source.colorPoints != nullptr) {
                next->colorPoints = *source.colorPoints;
            }

            if (source.curves != nullptr) {
                next->curves = *source.curves;
            }

            int size = source.waveform.waveX.size();

            if (size > 0) {
                next->buffer.ensureSize(size * 2);
                next->waveX = next->buffer.place(size);
                next->waveY = next->buffer.place(size);
                next->oneIndex = source.waveform.oneIndex;
                next->zeroIndex = source.waveform.zeroIndex;

                source.waveform.waveX.copyTo(next->waveX);
                source.waveform.waveY.copyTo(next->waveY);
            }

            target.publish(std::shared_ptr<const RasterizerSnapshotData>(std::move(next)));
        }
    };
}
