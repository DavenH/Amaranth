#pragma once

#include "../Policies/GuideCurvePolicy.h"
#include "../RasterizationRequest.h"
#include "../RasterizerComposer.h"
#include "../../Mesh.h"
#include "../../VertCube.h"

namespace Rasterization {
    class MeshSliceRenderer {
    public:
        struct Context {
            Mesh* mesh {};
            RasterizationRequest request;
            GuideCurveApplier* guideApplier {};
            VertCube::ReductionData* reduction {};
            float oscPhase {};
        };

        MeshSlicePipeline::Output render(const Context& context) const {
            jassert(context.mesh != nullptr);
            jassert(context.guideApplier != nullptr);
            jassert(context.reduction != nullptr);

            MeshCubeSource source(context.mesh);

            auto composedRasterizer = RasterizerComposer::mesh()
                    .withSource(source)
                    .withRequest(context.request)
                    .build();

            return composedRasterizer.renderWithReduction(
                    context.oscPhase,
                    *context.guideApplier,
                    *context.reduction);
        }
    };
}
