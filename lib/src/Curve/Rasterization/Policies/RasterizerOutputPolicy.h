#pragma once

#include "../Pipelines/FxRasterizationPipeline.h"
#include "../Pipelines/PointListRasterizationPipeline.h"
#include "../RasterizerRuntime.h"

namespace Rasterization {
    enum class WaveformPublication {
        Assign,
        Copy
    };

    inline void publishWaveform(
            const WaveformBuffers& source,
            const WaveformBufferRefs& target,
            WaveformPublication publication) {
        if (publication == WaveformPublication::Copy) {
            target.copyFrom(source);
        } else {
            target.assignFrom(source);
        }
    }

    class PointListOutputPolicy {
    public:
        explicit PointListOutputPolicy(WaveformPublication publication = WaveformPublication::Copy) :
                publication(publication) {
        }

        void publish(const PointListRasterizationPipeline::Output& output, RasterizerRuntime runtime) const {
            jassert(runtime.isBound());
            jassert(runtime.frontPadding != nullptr);
            jassert(runtime.backPadding != nullptr);

            *runtime.frontPadding = output.frontPadding;
            *runtime.backPadding = output.backPadding;
            *runtime.curves = output.curves;
            *runtime.paddingSize = output.paddingSize;
            *runtime.unsampleable = !output.sampleable;

            publishWaveform(output.waveform, runtime.waveform, publication);
        }

    private:
        WaveformPublication publication;
    };

    class FxOutputPolicy {
    public:
        explicit FxOutputPolicy(WaveformPublication publication = WaveformPublication::Assign) :
                publication(publication) {
        }

        void publish(const FxRasterizationPipeline::Output& output, RasterizerRuntime runtime) const {
            jassert(runtime.isBound());
            jassert(runtime.intercepts != nullptr);

            *runtime.intercepts = output.intercepts;
            *runtime.curves = output.curves;
            *runtime.paddingSize = output.paddingSize;
            *runtime.unsampleable = !output.sampleable;

            publishWaveform(output.waveform, runtime.waveform, publication);
        }

    private:
        WaveformPublication publication;
    };
}
