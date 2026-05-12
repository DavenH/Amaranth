#pragma once

#include <vector>

#include <App/SingletonAccessor.h>

#include "Mesh.h"
#include "Rasterization/BaseRasterizer.h"
#include "Rasterization/PointListWaveformRasterizer.h"
#include "Rasterization/RenderResult.h"
#include "Rasterization/RasterizationRequest.h"
#include "../Inter/Dimensions.h"

using std::vector;

class FXRasterizer :
        public SingletonAccessor
    ,   public Rasterization::BaseRasterizer {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());

    void cleanUp();
    void updateGeometry() override;
    void updateWaveform() override;
    void reset() override;

    void setVertices(vector<Vertex*>* vertices);

    bool canRasterizeWaveform() const;
    bool isBipolar() const;

    Rasterization::SamplerView sampler() const override {
        return pointListRasterizer.sampler();
    }

    void setMesh(Mesh* mesh);

    void setDims(const Dimensions& dims) { this->dims = dims; }
    void setScalingMode(int type) { scalingType = type; }

private:
    Rasterization::RasterizationRequest createFxRequest() const;
    Rasterization::RenderResult& result() { return pointListRasterizer.result(); }
    const Rasterization::RenderResult& result() const { return pointListRasterizer.result(); }
    Intercept interceptAt(Vertex* vertex) const;
    bool updateFxGeometry(const Rasterization::RasterizationRequest& request);
    void copyVertexInterceptsTo(vector<Intercept>& intercepts) const;
    void publishSnapshot();
    int vertexCount() const;

    Mesh* mesh {};
    Dimensions dims { Vertex::Phase, Vertex::Amp };
    vector<Vertex*>* vertices {};
    Rasterization::PointListWaveformRasterizer pointListRasterizer;

    int scalingType { Unipolar };
};
