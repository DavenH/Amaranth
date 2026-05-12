#pragma once

#include <vector>

#include <App/SingletonAccessor.h>

#include "Mesh.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/BaseRasterizer.h"
#include "Rasterization/Builders/CurveWaveformBuilder.h"
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

    Rasterization::SamplerView samplerView() const override {
        return Rasterization::SamplerView(result.waveform, result.sampleable);
    }

    void setMesh(Mesh* mesh);

    void setDims(const Dimensions& dims) { this->dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) { guideCurveProvider = provider; }
    void setScalingMode(int type) { scalingType = type; }

private:
    Rasterization::RasterizationRequest createFxRequest() const;
    Intercept interceptAt(Vertex* vertex) const;
    bool updateFxGeometry(const Rasterization::RasterizationRequest& request);
    void bakeWaveform(const Rasterization::RasterizationRequest& request);
    void copyVertexInterceptsTo(vector<Intercept>& intercepts) const;
    void publishSnapshot();
    int vertexCount() const;

    Mesh* mesh {};
    GuideCurveProvider* guideCurveProvider {};
    Dimensions dims { Vertex::Phase, Vertex::Amp };
    vector<Vertex*>* vertices {};
    Rasterization::RenderResult result;
    Rasterization::CurveWaveformBuilder curveWaveformBuilder;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;

    int scalingType { Unipolar };
};
