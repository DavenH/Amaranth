#pragma once

#include <vector>

#include <App/SingletonAccessor.h>

#include "Mesh.h"
#include "RasterizerData.h"
#include "Rasterization/Interfaces/GuideCurveBindableRasterizer.h"
#include "Rasterization/Interfaces/MeshBindableRasterizer.h"
#include "Rasterization/Interfaces/RasterizerSampler.h"
#include "Rasterization/Interfaces/RasterizerSnapshotProvider.h"
#include "Rasterization/Interfaces/RasterizerUpdateTarget.h"
#include "Rasterization/Interfaces/RasterizerVertexDomain.h"
#include "Rasterization/GuideCurveOffsetSeeds.h"
#include "Rasterization/Pipelines/CurveWaveformPipeline.h"
#include "Rasterization/RenderResult.h"
#include "Rasterization/RasterizationRequest.h"
#include "Rasterization/Sources/VertexListSource.h"
#include "../Design/Updating/Updateable.h"
#include "../Inter/Dimensions.h"

using std::vector;

class FXRasterizer :
        public Updateable
    ,   public SingletonAccessor
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain {
    JUCE_LEAK_DETECTOR(FXRasterizer)

public:
    enum ScalingType { Unipolar, Bipolar, HalfBipolar };

    explicit FXRasterizer(SingletonRepo* repo, const String& name = String());

    void calcCrossPoints();
    void cleanUp() override;
    Rasterization::RasterizationRequest createRasterizationRequest() const;
    void makeCopy();
    void performUpdate(UpdateType updateType) override;
    void reset() override;
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    void setVertices(vector<Vertex*>* vertices);

    bool canRasterizeWaveform() const;
    bool hasEnoughCubesForCrossSection() override;
    bool isBipolar() const;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    void updateWaveform(UpdateType updateType);
    bool wrapsVertices() const override { return false; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    double sampleWithInterval(Buffer<float> buffer, double delta, double phase);
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;
    void sampleEvenlyTo(const Buffer<float>& dest);

    Mesh* getMesh() override { return mesh; }
    void setMesh(Mesh* mesh) override;
    void padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves);
    int getNumDims() { return 1; }
    int getPaddingSize() const override { return result.paddingSize; }
    int getOneIndex() const { return result.waveform.oneIndex; }
    int getZeroIndex() const { return result.waveform.zeroIndex; }
    GuideCurveProvider* getGuideCurveProvider() const override { return guideCurveProvider; }
    const vector<Curve>& getCurves() const { return result.curves; }
    vector<ColorPoint>& getColorPoints() { return result.colorPoints; }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }
    RasterizerData& getRastData() { return rasterizerData; }

    void setDims(const Dimensions& dims) override { this->dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }
    void setScalingMode(int type) { scalingType = type; }

    Buffer<float> getWaveX() { return result.waveform.waveX; }
    Buffer<float> getWaveY() { return result.waveform.waveY; }
    Buffer<float> getSlopes() { return result.waveform.slope; }
    Buffer<float> getDiffX() { return result.waveform.diffX; }

private:
    Rasterization::RasterizationRequest createFxRequest() const;
    bool renderFx(const Rasterization::RasterizationRequest& request);
    void bakeWaveform(const Rasterization::RasterizationRequest& request);
    void publishSnapshot();

    Mesh* mesh {};
    GuideCurveProvider* guideCurveProvider {};
    Dimensions dims { Vertex::Phase, Vertex::Amp };
    Rasterization::VertexListSource source;
    Rasterization::RenderResult result;
    RasterizerData rasterizerData;
    Rasterization::CurveWaveformPipeline curveWaveformPipeline;
    Rasterization::GuideCurveOffsetSeeds guideCurveOffsetSeeds;

    int scalingType { Unipolar };
};
