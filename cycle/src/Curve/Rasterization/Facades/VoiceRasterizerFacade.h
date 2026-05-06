#pragma once

#include "../Policies/VoiceChainedPaddingPolicy.h"
#include "../Policies/VoicePointPositionPolicy.h"

namespace Cycle::Rasterization {
    class VoiceRasterizerFacade {
    public:
        VoicePointPositionPolicy::Result resolvePoint(
                const VoicePointPositionPolicy::Context& context,
                bool pointOverlaps,
                Vertex* a,
                Vertex* b,
                Vertex* vertex) const {
            return pointPositionPolicy.resolve(context, pointOverlaps, a, b, vertex);
        }

        int buildChainedPadding(
                std::vector<Intercept>& intercepts,
                std::vector<Intercept>& nextIntercepts,
                CycleState& state,
                std::vector<Curve>& curves,
                float interceptPadding) const {
            return chainedPaddingPolicy.build(intercepts, nextIntercepts, state, curves, interceptPadding);
        }

    private:
        VoicePointPositionPolicy pointPositionPolicy;
        VoiceChainedPaddingPolicy chainedPaddingPolicy;
    };
}
