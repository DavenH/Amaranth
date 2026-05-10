#pragma once

#include <algorithm>
#include <vector>

#include <Curve/Intercept.h>

#include "../../../CycleState.h"
#include "../../Pipelines/VoiceSlicePipeline.h"

namespace Cycle::Rasterization {
    class VoiceChainingPolicy {
    public:
        explicit VoiceChainingPolicy(bool* needsResorting) :
                needsResorting(needsResorting) {
        }

        void beginCall(CycleState& state, std::vector<Intercept>& currentIntercepts) const {
            if (state.callCount > 0) {
                std::swap(state.backIcpts, currentIntercepts);
                state.backIcpts.clear();
            }

            if (needsResorting != nullptr) {
                *needsResorting = false;
            }
        }

        template<typename RestrictIntercepts>
        bool publishNextIntercepts(
                const VoiceSlicePipeline::Output& output,
                CycleState& state,
                RestrictIntercepts&& restrictIntercepts) const {
            state.backIcpts = output.intercepts;

            restrictIntercepts(state.backIcpts);
            sortIfNeeded(state.backIcpts);

            return state.backIcpts.size() >= 2;
        }

        bool canBuildChainedCurves(
                const CycleState& state,
                const std::vector<Intercept>& currentIntercepts) const {
            return state.backIcpts.size() >= 2 && currentIntercepts.size() >= 2;
        }

    private:
        void sortIfNeeded(std::vector<Intercept>& intercepts) const {
            if (needsResorting == nullptr || !*needsResorting) {
                return;
            }

            std::sort(intercepts.begin(), intercepts.end());
            *needsResorting = false;
        }

        bool* needsResorting {};
    };
}
