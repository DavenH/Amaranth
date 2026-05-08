#pragma once

#include <vector>

#include "WaveformSampler.h"
#include "../../GuideCurveProvider.h"
#include "../../Intercept.h"
#include "../WaveformBuffers.h"
#include "../../../Util/NumberUtils.h"

namespace Rasterization {
    struct GuideCurveContext {
        int phaseOffsetSeed {};
        int vertOffsetSeed {};
        int currentIndex {};
    };

    struct GuideCurveRegion {
        int guideIndex {};
        float amplitude {};
        Intercept start;
        Intercept end;
    };

    class GuideCurveSampler {
    public:
        static float sampleDecoupled(
                const WaveformBuffers& waveform,
                bool unsampleable,
                double angle,
                GuideCurveContext& context,
                const std::vector<GuideCurveRegion>& regions,
                GuideCurveProvider* guideCurveProvider,
                int noiseSeed) {
            float value = WaveformSampler::sampleAt(waveform, unsampleable, angle, context.currentIndex);

            if (guideCurveProvider == nullptr) {
                return value;
            }

            for (const auto& region : regions) {
                if (NumberUtils::within<float>(angle, region.start.x, region.end.x)) {
                    float diff = region.end.x - region.start.x;

                    if (diff > 0) {
                        GuideCurveProvider::NoiseContext noise;
                        noise.noiseSeed   = noiseSeed;
                        noise.phaseOffset = context.phaseOffsetSeed;
                        noise.vertOffset  = context.vertOffsetSeed;

                        float progress = (angle - region.start.x) / diff;

                        return value
                                + region.amplitude
                                * guideCurveProvider->getTableValue(region.guideIndex, progress, noise);
                    }
                }
            }

            return value;
        }
    };
}
