#pragma once

#include "WaveformBakePolicy.h"

namespace Rasterization {
    class WaveformBuildPolicy {
    public:
        template<typename AllocateTarget>
        bool build(
                std::vector<Curve>& curves,
                WaveformBakePolicy::Context& context,
                AllocateTarget allocateTarget) const {
            int totalRes = bakePolicy.prepare(curves, context);
            if (totalRes <= 0) {
                return false;
            }

            context.waveform = allocateTarget(totalRes);
            bakePolicy.bake(curves, context);

            return context.waveform.isSampleable();
        }

    private:
        WaveformBakePolicy bakePolicy;
    };
}
