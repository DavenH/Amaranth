#pragma once

#include <utility>

#include <Curve/Mesh.h>
#include <Curve/Rasterization/RasterizerRuntime.h>
#include <Curve/Rasterization/Sources/MeshCubeSource.h>
#include <Curve/VertCube.h>
#include <Obj/MorphPosition.h>

#include "../../CycleState.h"
#include "../Facades/VoiceRasterizerFacade.h"
#include "../Policies/VoiceChainingPolicy.h"

namespace Cycle::Rasterization {
    class VoiceRasterizationPipeline {
    public:
        struct Context {
            Mesh* mesh {};
            CycleState* state {};
            MorphPosition morph;
            ::Rasterization::RasterizerRuntime runtime;
            VertCube::ReductionData* reductionData {};

            float oscPhase {};
            float interceptPadding {};
            float initialAdvancement {};

            bool canRasterize() const {
                return mesh != nullptr
                    && mesh->getNumCubes() > 0
                    && state != nullptr
                    && reductionData != nullptr
                    && runtime.intercepts != nullptr;
            }
        };

        template<
                typename GuideApplier,
                typename RestrictIntercepts,
                typename MarkUnsampleable,
                typename UpdateCurves>
        bool render(
                Context context,
                GuideApplier&& applyGuide,
                RestrictIntercepts&& restrictIntercepts,
                MarkUnsampleable&& markUnsampleable,
                UpdateCurves&& updateCurves) const {
            if (!context.canRasterize()) {
                return false;
            }

            VoiceChainingPolicy chainingPolicy(context.runtime.needsResorting);
            chainingPolicy.beginCall(*context.state, context.runtime);

            ::Rasterization::MeshCubeSource source(context.mesh);
            auto output = facade.buildIntercepts(
                    source,
                    context.morph,
                    context.state->advancement,
                    context.oscPhase,
                    std::forward<GuideApplier>(applyGuide),
                    *context.reductionData);

            chainingPolicy.publishNextIntercepts(
                    output,
                    *context.state,
                    std::forward<RestrictIntercepts>(restrictIntercepts));

            if (!chainingPolicy.canBuildChainedCurves(*context.state, context.runtime)) {
                ++context.state->callCount;
                markUnsampleable(context.runtime);

                return true;
            }

            if (context.state->callCount == 0) {
                context.state->advancement = context.initialAdvancement;
            }

            if (context.state->callCount > 0) {
                facade.buildChainedPadding(
                        *context.state,
                        context.runtime,
                        context.interceptPadding);

                updateCurves();
            }

            ++context.state->callCount;

            return true;
        }

    private:
        VoiceRasterizerFacade facade;
    };
}
