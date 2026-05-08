#pragma once

#include <Curve/Rasterization/RasterizerRuntime.h>
#include <Curve/Rasterization/Sources/MeshCubeSource.h>

#include "../Pipelines/VoiceSlicePipeline.h"
#include "../Policies/VoiceChainedPaddingPolicy.h"
#include "../Policies/VoiceCurveResolutionPolicy.h"
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

        template<typename GuideApplier>
        VoiceSlicePipeline::Output buildIntercepts(
                const ::Rasterization::MeshCubeSource& source,
                const MorphPosition& morph,
                float advancement,
                float oscPhase,
                GuideApplier&& applyGuide,
                VertCube::ReductionData& reductionData) const {
            return slicePipeline.render(
                    source,
                    morph,
                    advancement,
                    oscPhase,
                    applyGuide,
                    reductionData);
        }

        int buildChainedPadding(
                std::vector<Intercept>& intercepts,
                std::vector<Intercept>& nextIntercepts,
                CycleState& state,
                std::vector<Curve>& curves,
                float interceptPadding) const {
            return chainedPaddingPolicy.build(intercepts, nextIntercepts, state, curves, interceptPadding);
        }

        int buildChainedPadding(
                std::vector<Intercept>& intercepts,
                std::vector<Intercept>& nextIntercepts,
                CycleState& state,
                ::Rasterization::RasterizerRuntime runtime,
                float interceptPadding) const {
            jassert(runtime.curves != nullptr);
            jassert(runtime.paddingSize != nullptr);

            *runtime.paddingSize = chainedPaddingPolicy.build(
                    intercepts,
                    nextIntercepts,
                    state,
                    *runtime.curves,
                    interceptPadding);

            return *runtime.paddingSize;
        }

        int buildChainedPadding(
                CycleState& state,
                ::Rasterization::RasterizerRuntime runtime,
                float interceptPadding) const {
            jassert(runtime.intercepts != nullptr);
            jassert(runtime.curves != nullptr);
            jassert(runtime.paddingSize != nullptr);

            *runtime.paddingSize = chainedPaddingPolicy.build(
                    *runtime.intercepts,
                    state.backIcpts,
                    state,
                    *runtime.curves,
                    interceptPadding);

            return *runtime.paddingSize;
        }

        void applyCurveResolution(std::vector<Curve>& curves) const {
            curveResolutionPolicy.apply(curves);
        }

        void applyCurveResolution(::Rasterization::RasterizerRuntime runtime) const {
            jassert(runtime.curves != nullptr);

            curveResolutionPolicy.apply(*runtime.curves);
        }

    private:
        VoicePointPositionPolicy pointPositionPolicy;
        VoiceChainedPaddingPolicy chainedPaddingPolicy;
        VoiceSlicePipeline slicePipeline;
        VoiceCurveResolutionPolicy curveResolutionPolicy;
    };
}
