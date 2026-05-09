#pragma once

#include <Curve/Rasterization/RasterizerComposer.h>

class Mesh;

namespace Cycle::Rasterization {
    class OscPhaseRasterizer {
    public:
        OscPhaseRasterizer() = default;

        void rasterize(Mesh* mesh) {
            auto rasterizer = ::Rasterization::RasterizerComposer::mesh()
                    .withSource(::Rasterization::MeshCubeSource(mesh))
                    .withSlicer(::Rasterization::TrilinearMeshSlicer())
                    .withRequest(request)
                    .build();

            output = rasterizer.render(0.f, [](Intercept&, const MorphPosition&, bool) {});
        }

    private:
        ::Rasterization::RasterizationRequest request;
        ::Rasterization::MeshSlicePipeline::Output output;
    };
}
