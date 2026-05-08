#pragma once

#include "../Builders/WaveformBuilder.h"

namespace Rasterization {
    class WaveformBuildPolicy {
    public:
        template<typename AllocateTarget>
        bool build(
                std::vector<Curve>& curves,
                WaveformBakePolicy::Context& context,
                AllocateTarget allocateTarget) const {
            int totalRes = waveformBuilder.prepare(curves, context);
            if (totalRes <= 0) {
                return false;
            }

            context.waveform = allocateTarget(totalRes);
            waveformBuilder.bake(curves, context);

            return context.waveform.isSampleable();
        }

    private:
        WaveformBuilder waveformBuilder;
    };
}
