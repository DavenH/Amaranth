#pragma once

#include <vector>

#include "../Pipelines/MeshSlicePipeline.h"
#include "../../Intercept.h"
#include "../../../Obj/ColorPoint.h"

namespace Rasterization {
    class MeshSliceOutputPolicy {
    public:
        explicit MeshSliceOutputPolicy(bool publishColorPoints) :
                publishColorPoints(publishColorPoints) {
        }

        void publish(
                const MeshSlicePipeline::Output& output,
                std::vector<Intercept>& intercepts,
                std::vector<ColorPoint>& colorPoints) const {
            intercepts = output.intercepts;

            if (publishColorPoints) {
                colorPoints = output.colorPoints;
            }
        }

    private:
        bool publishColorPoints {};
    };
}
