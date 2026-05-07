#pragma once

#include <memory>

#include <App/SingletonAccessor.h>

#include "MeshRasterizer.h"
#include "Rasterization/RasterizerComposer.h"
#include "Rasterization/Sources/VertexListSource.h"

class FXRasterizer:
        public MeshRasterizer
    ,   public SingletonAccessor {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
    bool hasEnoughCubesForCrossSection() override;
    int  getNumDims() override;
    void calcCrossPoints() override;
    void cleanUp() override;
    void padIcpts(vector<Intercept>& icpts, vector<Curve>& curves) override;
    void setVertices(vector<Vertex*>* vertices);
    void setMesh(Mesh* newMesh) override;

private:
    Rasterization::RasterizationRequest createFxRequest();
    void publishPipelineOutput(const Rasterization::FxRasterizationPipeline::Output& output);

    Rasterization::VertexListSource vertexSource;
    std::unique_ptr<Rasterization::ComposedFxRasterizer> composedRasterizer;
};
