#pragma once

#include <App/SingletonAccessor.h>

#include "MeshRasterizer.h"
#include "Rasterization/Adapters/FxRasterizerAdapter.h"

class FXRasterizer:
        public MeshRasterizer
    ,   public SingletonAccessor {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());
    void calcCrossPoints() override;
    void setVertices(vector<Vertex*>* vertices);
    void setMesh(Mesh* newMesh) override;

private:
    Rasterization::RasterizationRequest createFxRequest();

    Rasterization::FxRasterizerAdapter adapter;
};
