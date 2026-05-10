#pragma once

#include <vector>

#include <App/SingletonAccessor.h>

#include "Mesh.h"
#include "RasterizerData.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/Rasterizer.h"
#include "Rasterization/Builders/CurveWaveformBuilder.h"
#include "Rasterization/RenderResult.h"
#include "Rasterization/RasterizationRequest.h"
#include "../Design/Updating/Updateable.h"
#include "../Inter/Dimensions.h"

using std::vector;

class FXRasterizer :
        public Updateable
    ,   public SingletonAccessor
    ,   public Rasterization::Rasterizer {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());

    void calcCrossPoints();
    void cleanUp();
    void performUpdate(UpdateType updateType) override;
    void reset() override;

    void setVertices(vector<Vertex*>* vertices);

    bool canRasterizeWaveform() const;
    bool hasEnoughCubesForCrossSection();
    bool isBipolar() const;
    void updateWaveform(UpdateType updateType);
    bool wrapsVertices() const override { return false; }

    Rasterization::SamplerView samplerView() const override {
        return Rasterization::SamplerView(result.waveform, result.sampleable);
    }
    Rasterization::SnapshotView snapshotView() override { return Rasterization::SnapshotView(rasterizerData); }
    void sampleEvenlyTo(const Buffer<float>& dest);

    Mesh* getMesh() { return mesh; }
    void setMesh(Mesh* mesh) override;
    int getPaddingSize() const override { return result.paddingSize; }
    GuideCurveProvider* getGuideCurveProvider() const override { return guideCurveProvider; }
    const Rasterization::RenderResult& getRenderResult() const { return result; }

    void setDims(const Dimensions& dims) override { this->dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }
    void setScalingMode(int type) { scalingType = type; }

private:
    Rasterization::RasterizationRequest createFxRequest() const;
    Intercept interceptAt(Vertex* vertex) const;
    bool renderFx(const Rasterization::RasterizationRequest& request);
    void bakeWaveform(const Rasterization::RasterizationRequest& request);
    void copyVertexInterceptsTo(vector<Intercept>& intercepts) const;
    void publishSnapshot();
    int vertexCount() const;

    Mesh* mesh {};
    GuideCurveProvider* guideCurveProvider {};
    Dimensions dims { Vertex::Phase, Vertex::Amp };
    vector<Vertex*>* vertices {};
    Rasterization::RenderResult result;
    RasterizerData rasterizerData;
    Rasterization::CurveWaveformBuilder curveWaveformBuilder;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;

    int scalingType { Unipolar };
};
