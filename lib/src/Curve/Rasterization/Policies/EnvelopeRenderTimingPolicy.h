#pragma once

#include <App/MeshLibrary.h>
#include <Definitions.h>

namespace Rasterization {
    struct EnvelopeRenderTimingContext {
        int numSamples {};
        double deltaX {};
        float tempoScale { 1.f };
        float loopLength { -1.f };
        const MeshLibrary::EnvProps* props {};
    };

    struct EnvelopeRenderTiming {
        double effectiveDelta {};
        int maxSamplesPerBuffer { 1 };
    };

    class EnvelopeRenderTimingPolicy {
    public:
        EnvelopeRenderTiming prepare(const EnvelopeRenderTimingContext& context) const {
            jassert(context.deltaX > 0);
            jassert(context.props != nullptr);

            EnvelopeRenderTiming result;
            result.effectiveDelta = context.deltaX;

            if (context.props == nullptr) {
                return result;
            }

            if (context.props->tempoSync) {
                result.effectiveDelta /= context.tempoScale;
            }

            if (context.props->scale != 1) {
                result.effectiveDelta /= (double) context.props->getEffectiveScale();
            }

            double loopLength = jmax<double>(2. * result.effectiveDelta, context.loopLength);
            double maxIterativeAdvancement = 0.5;

            if (loopLength > 2. * result.effectiveDelta) {
                maxIterativeAdvancement = 0.9 * loopLength;
            }

            result.maxSamplesPerBuffer = jlimit(
                    1,
                    512,
                    int(maxIterativeAdvancement / result.effectiveDelta));

            return result;
        }
    };
}
