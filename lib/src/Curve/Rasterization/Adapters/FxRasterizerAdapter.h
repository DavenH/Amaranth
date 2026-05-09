#pragma once

#include <memory>
#include <vector>

#include "../Policies/Core/PaddingPolicy.h"
#include "../Policies/Core/RasterizerCleanupPolicy.h"
#include "../Policies/Core/RasterizerOutputPolicy.h"
#include "../RasterizerComposer.h"
#include "../RasterizerRuntime.h"
#include "../Sources/VertexListSource.h"
#include "../../Mesh.h"

namespace Rasterization {
    class FxRasterizerAdapter {
    public:
        FxRasterizerAdapter() {
            source.setDimensions(Vertex::Phase, Vertex::Amp);
        }

        void setMesh(Mesh* mesh) {
            source.setVertices(mesh == nullptr ? nullptr : &mesh->getVerts());
        }

        void setVertices(std::vector<Vertex*>* vertices) {
            source.setVertices(vertices);
        }

        bool hasEnoughCubesForCrossSection() const {
            return source.size() > 1;
        }

        int sourceSize() const {
            return source.size();
        }

        const VertexListSource& getSource() const {
            return source;
        }

        bool render(const RasterizationRequest& request, RasterizerRuntime runtime) {
            if (source.empty()) {
                clean(runtime);
                return false;
            }

            composedRasterizer.reset(new ComposedFxRasterizer(
                    RasterizerComposer::fx()
                            .withSource(source)
                            .withRequest(request)
                            .build()));

            const auto& output = composedRasterizer->render();
            if (!output.sampleable) {
                clean(runtime);
                return false;
            }

            FxOutputPolicy(WaveformPublication::Assign).publish(output, runtime);
            return true;
        }

        void clean(RasterizerRuntime runtime) {
            RasterizerCleanupOptions options;
            options.clearFrontPadding = false;
            options.clearBackPadding = false;
            options.clearColorPoints = false;

            RasterizerCleanupPolicy(options).clean(runtime);
            composedRasterizer.reset();
        }

        int buildPadding(
                std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves,
                int& paddingSize) const {
            paddingSize = FxPaddingPolicy().build(intercepts, curves);
            return paddingSize;
        }

    private:
        VertexListSource source;
        std::unique_ptr<ComposedFxRasterizer> composedRasterizer;
    };
}
