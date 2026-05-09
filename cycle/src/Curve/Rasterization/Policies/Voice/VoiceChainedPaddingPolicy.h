#pragma once

#include <vector>

#include <Curve/Curve.h>
#include <Curve/Intercept.h>
#include <Curve/Rasterization/Policies/Curves/CurveTripletPolicy.h>

#include "../../../CycleState.h"

namespace Cycle::Rasterization {
    class VoiceChainedPaddingPolicy {
    public:
        int build(
                const std::vector<Intercept>& intercepts,
                const std::vector<Intercept>& nextIntercepts,
                CycleState& state,
                std::vector<Curve>& curves,
                float interceptPadding) const {
            int end = (int) intercepts.size() - 1;

            if (end < 1 || nextIntercepts.size() < 2) {
                return 0;
            }

            Intercept back1, back2, back3, back4, back5;
            back1.assignWithTranslation(nextIntercepts[0], 1.f);
            back2.assignWithTranslation(nextIntercepts[1], 1.f);
            assignBack(back3, nextIntercepts, 3);
            assignBack(back4, nextIntercepts, 4);
            assignBack(back5, nextIntercepts, 5);

            bool extraPadFront = state.frontB.x > -4.f * interceptPadding;
            bool padFront      = state.frontA.x > -4.f * interceptPadding;
            bool padBack       = back1.x < 1.f + 4.f * interceptPadding;
            bool extraPadBack  = back2.x < 1.f + 4.f * interceptPadding;

            curves.clear();
            curves.reserve(intercepts.size() + 5 + int(extraPadFront) + int(padFront) + int(padBack) + int(extraPadBack));

            if (extraPadFront) {
                curves.emplace_back(state.frontE, state.frontD, state.frontC);
            }

            if (padFront) {
                curves.emplace_back(state.frontD, state.frontC, state.frontB);
            }

            curves.emplace_back(state.frontC, state.frontB, state.frontA);
            curves.emplace_back(state.frontB, state.frontA, intercepts[0]);
            curves.emplace_back(state.frontA, intercepts[0], intercepts[1]);

            ::Rasterization::CurveTripletPolicy::appendAdjacent(intercepts, curves);

            curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
            curves.emplace_back(intercepts[end], back1, back2);
            curves.emplace_back(back1, back2, back3);

            if (padBack) {
                curves.emplace_back(back2, back3, back4);
            }

            if (extraPadBack) {
                curves.emplace_back(back3, back4, back5);
            }

            updateFrontState(intercepts, state);
            return 2;
        }

    private:
        static void updateFrontState(const std::vector<Intercept>& intercepts, CycleState& state) {
            int end = (int) intercepts.size() - 1;

            assignFront(state.frontE, intercepts, 5);
            assignFront(state.frontD, intercepts, 4);
            assignFront(state.frontC, intercepts, 3);
            state.frontB.assignWithTranslation(intercepts[end - 1], -1.f);
            state.frontA.assignWithTranslation(intercepts[end], -1.f);
        }

        static void assignBack(Intercept& dest, const std::vector<Intercept>& intercepts, int index) {
            int size = (int) intercepts.size();
            int offset = (index - 1) / size + 1;
            int localIndex = index - size * (offset - 1) - 1;

            dest = intercepts[localIndex];
            dest.x += offset;
        }

        static void assignFront(Intercept& dest, const std::vector<Intercept>& intercepts, int index) {
            int size = (int) intercepts.size();
            int offset = (index - 1) / size + 1;
            int localIndex = size - 1 - (index - 1 - (offset - 1) * size);

            dest = intercepts[localIndex];
            dest.x -= offset;
        }
    };
}
