#pragma once

#include "../Pipelines/MeshSlicePipeline.h"
#include "../RasterizerRuntime.h"

namespace Rasterization {
    class MeshSliceOutputPolicy {
    public:
        explicit MeshSliceOutputPolicy(bool publishColorPoints) :
                publishColorPoints(publishColorPoints) {
        }

        void publish(const MeshSlicePipeline::Output& output, RasterizerRuntime runtime) const {
            jassert(runtime.intercepts != nullptr);

            *runtime.intercepts = output.intercepts;

            if (publishColorPoints) {
                jassert(runtime.colorPoints != nullptr);
                *runtime.colorPoints = output.colorPoints;
            }
        }

    private:
        bool publishColorPoints {};
    };
}
