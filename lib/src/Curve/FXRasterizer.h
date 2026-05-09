#pragma once

#include <vector>

#include <App/SingletonAccessor.h>

#include "Mesh.h"
#include "RasterizerData.h"
#include "Rasterization/Adapters/FxRasterizerAdapter.h"
#include "Rasterization/Interfaces/GuideCurveBindableRasterizer.h"
#include "Rasterization/Interfaces/MeshBindableRasterizer.h"
#include "Rasterization/Interfaces/RasterizerSampler.h"
#include "Rasterization/Interfaces/RasterizerSnapshotProvider.h"
#include "Rasterization/Interfaces/RasterizerUpdateTarget.h"
#include "Rasterization/Interfaces/RasterizerVertexDomain.h"
#include "Rasterization/Interfaces/WaveformProvider.h"
#include "Rasterization/RasterizationRequest.h"
#include "Rasterization/RasterizerRuntime.h"
#include "Rasterization/State/RasterizerStorage.h"
#include "../Array/ScopedAlloc.h"
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
    ,   public Rasterization::RasterizerVertexDomain
    ,   public Rasterization::WaveformProvider {
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

    bool canRasterizeWaveform() const override;
    bool hasEnoughCubesForCrossSection() override;
    bool isBipolar() const override;
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    void updateWaveform(UpdateType updateType) override;
    bool wrapsVertices() const override { return false; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    double sampleWithInterval(Buffer<float> buffer, double delta, double phase) override;
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override;
    void sampleEvenlyTo(const Buffer<float>& dest);

    Mesh* getMesh() override { return mesh; }
    void setMesh(Mesh* mesh) override;
    void padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves);
    int getNumDims() { return 1; }
    int getPaddingSize() const override { return paddingSize; }
    int getOneIndex() const { return storage.waveform.waveform.oneIndex; }
    int getZeroIndex() const { return storage.waveform.waveform.zeroIndex; }
    GuideCurveProvider* getGuideCurveProvider() const override { return guideCurveProvider; }
    const vector<Curve>& getCurves() const { return storage.curves.curves; }
    vector<ColorPoint>& getColorPoints() { return storage.intercepts.colorPoints; }
    RasterizerData& getRasterizerData() override { return storage.snapshot.rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return storage.snapshot.rasterizerData; }
    RasterizerData& getRastData() { return storage.snapshot.rasterizerData; }

    void setDims(const Dimensions& dims) override { this->dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { guideCurveProvider = provider; }
    void setScalingMode(int type) { scalingType = type; }

    Buffer<float> getWaveX() { return storage.waveform.waveform.waveX; }
    Buffer<float> getWaveY() { return storage.waveform.waveform.waveY; }
    Buffer<float> getSlopes() { return storage.waveform.waveform.slope; }
    Buffer<float> getDiffX() { return storage.waveform.waveform.diffX; }

private:
    Rasterization::RasterizationRequest createFxRequest() const;
    Rasterization::RasterizerRuntime createRasterizerRuntime();
    void publishSnapshot();

    Mesh* mesh {};
    GuideCurveProvider* guideCurveProvider {};
    Dimensions dims { Vertex::Phase, Vertex::Amp };
    Rasterization::RasterizerStorage storage;
    ScopedAlloc<float> memoryBuffer;
    Rasterization::FxRasterizerAdapter adapter;

    int paddingSize { 1 };
    int scalingType { Unipolar };
    bool unsampleable { true };
    bool needsResorting {};
};
