#pragma once

#include "../Policies/WaveformBakePolicy.h"

namespace Rasterization {
    class WaveformBuilder {
    public:
        int prepare(std::vector<Curve>& curves, const WaveformBakePolicy::Context& context) const {
            return bakePolicy.prepare(curves, context);
        }

        void bake(std::vector<Curve>& curves, WaveformBakePolicy::Context& context) const {
            bakePolicy.bake(curves, context);
        }

    private:
        WaveformBakePolicy bakePolicy;
    };
}
