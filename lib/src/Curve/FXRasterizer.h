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
    void setVertices(vector<Vertex*>* vertices);

private:
    Rasterization::RasterizationRequest createFxRequest();

    Rasterization::FxRasterizerAdapter adapter;
};
