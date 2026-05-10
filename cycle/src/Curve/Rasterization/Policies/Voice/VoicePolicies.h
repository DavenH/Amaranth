#pragma once

#include <algorithm>
#include <vector>

#include <Curve/Curve.h>
#include <Curve/Intercept.h>
#include <Curve/Rasterization/Policies/Curves/CurveResolutionPolicy.h>
#include <Curve/Rasterization/Policies/Curves/CurveTripletPolicy.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/VertCube.h>
#include <Curve/Vertex.h>

#include "../../../CycleState.h"

namespace Cycle::Rasterization {
    class VoicePointPositionPolicy {
    public:
        struct Context {
            float voiceTime {};
            float oscPhase {};
        };

        struct Result {
            bool intersects {};
            float phase {};
        };

        Result resolve(
                const Context& context,
                bool pointOverlaps,
                Vertex* a,
                Vertex* b,
                Vertex* vertex) const {
            if (!pointOverlaps) {
                return {};
            }

            jassert(a != nullptr && b != nullptr && vertex != nullptr);
            jassert(a->values[Vertex::Phase] >= 0.f);

            normalizeOverlapEndpoints(context.voiceTime, a, b);
            VertCube::vertexAt(context.voiceTime, Vertex::Time, a, b, vertex);

            return { true, wrapPhase(vertex->values[Vertex::Phase] + context.oscPhase) };
        }

    private:
        static void normalizeOverlapEndpoints(float voiceTime, Vertex* a, Vertex* b) {
            if (a->values[Vertex::Phase] > 1.f && b->values[Vertex::Phase] > 1.f) {
                a->values[Vertex::Phase] -= 1.f;
                b->values[Vertex::Phase] -= 1.f;
            }

            if ((a->values[Vertex::Phase] > 1.f) != (b->values[Vertex::Phase] > 1.f)) {
                float interceptTime = a->values[Vertex::Time]
                                    + (1.f - a->values[Vertex::Phase])
                                      / ((a->values[Vertex::Phase] - b->values[Vertex::Phase])
                                         / (a->values[Vertex::Time] - b->values[Vertex::Time]));

                if (interceptTime > voiceTime) {
                    a->values[Vertex::Phase] -= 1.f;
                    b->values[Vertex::Phase] -= 1.f;
                }
            }
        }

        static float wrapPhase(float phase) {
            while (phase >= 1.f) {
                phase -= 1.f;
            }

            while (phase < 0.f) {
                phase += 1.f;
            }

            jassert(phase >= 0.f && phase < 1.f);
            return phase;
        }
    };

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
                const ::Rasterization::RenderResult& output,
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

    class VoiceCurveResolutionPolicy {
    public:
        void apply(std::vector<Curve>& curves) const {
            if (curves.size() < 2) {
                return;
            }

            ::Rasterization::CurveResolutionPolicy::applyResolutionIndices(
                    curves,
                    0.05f / float(Curve::resolution),
                    1);

            int lastIdx = (int) curves.size() - 1;
            curves[0].resIndex = curves[1].resIndex;
            curves[lastIdx].resIndex = curves[lastIdx - 1].resIndex;
        }
    };
}
