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
    Rasterization::RasterizationRequest createRasterizationRequest() const;
    void makeCopy();
    void performUpdate(UpdateType updateType) override;
    void reset() override;
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    void setVertices(vector<Vertex*>* vertices);

    bool canRasterizeWaveform() const;
    bool hasEnoughCubesForCrossSection();
    bool isBipolar() const;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    void updateWaveform(UpdateType updateType);
    bool wrapsVertices() const override { return false; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    double sampleWithInterval(Buffer<float> buffer, double delta, double phase);
    float samplePerfectly(double delta, Buffer<float> buffer, double phase);
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

    void setDims(const Dimensions& dims) override { this->dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }
    void setScalingMode(int type) { scalingType = type; }

    Buffer<float> getWaveX() { return result.waveform.waveX; }
    Buffer<float> getWaveY() { return result.waveform.waveY; }
    Buffer<float> getSlopes() { return result.waveform.slope; }
    Buffer<float> getDiffX() { return result.waveform.diffX; }

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
