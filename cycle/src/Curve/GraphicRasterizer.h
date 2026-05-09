#pragma once

#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/ComposedMeshWaveformRasterizer.h>
#include <Curve/Rasterization/Interfaces/GuideCurveBindableRasterizer.h>
#include <Curve/Rasterization/Interfaces/MeshBindableRasterizer.h>
#include <Curve/Rasterization/Interfaces/RasterizerSampler.h>
#include <Curve/Rasterization/Interfaces/RasterizerSnapshotProvider.h>
#include <Curve/Rasterization/Interfaces/RasterizerUpdateTarget.h>
#include <Curve/Rasterization/Interfaces/RasterizerVertexDomain.h>
#include <Curve/Rasterization/MeshRasterizerState.h>
#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/RasterizerData.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "Rasterization/Facades/GraphicRasterizerFacade.h"
#include "../Updating/DynamicDetailUpdater.h"

class Interactor;

class GraphicRasterizer :
        public Updateable
    ,	public DynamicDetailUpdateable
    ,	public virtual SingletonAccessor
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain {
public:
    using RenderState = Rasterization::MeshRasterizerRenderState;

    enum class Scaling {
        Unipolar = 0,
        Bipolar = 1,
        HalfBipolar = 2
    };

    class ScopedRenderState {
    public:
        ScopedRenderState(GraphicRasterizer* rasterizer, RenderState* state);
        ~ScopedRenderState();

    private:
        GraphicRasterizer* rasterizer {};
        RenderState* state {};
    };

    GraphicRasterizer(SingletonRepo* repo,
                      Interactor* interactor,
                      const String& name, int layerGroup,
                      bool cyclic, float margin);

    void pullModPositionAndAdjust();
    void restoreStateFrom(RenderState& state);
    void saveStateTo(RenderState& state);
    ScopedRenderState preserveState(RenderState& state) { return ScopedRenderState(this, &state); }

    void calcCrossPoints(Mesh* mesh, float oscPhase);
    void cleanUp() override;
    void performUpdate(UpdateType updateType) override;
    void reset() override { cleanUp(); }
    void updateRasterizer(UpdateType updateType) override { Updateable::update(updateType); }

    bool hasEnoughCubesForCrossSection() override;
    bool isSampleable() override { return rasterizer.isSampleable(); }
    bool isSampleableAt(float x) override { return rasterizer.isSampleableAt(x); }
    bool wrapsVertices() const override { return rasterizer.getRequest().cyclic; }

    float sampleAt(double angle) override { return rasterizer.sampleAt(angle); }
    float sampleAt(double angle, int& currentIndex) override { return rasterizer.sampleAt(angle, currentIndex); }
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
        return rasterizer.samplePerfectly(delta, buffer, phase);
    }
    void sampleAtIntervals(Buffer<float> deltas, Buffer<float> dest) { rasterizer.sampleAtIntervals(deltas, dest); }

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        return rasterizer.sampleWithInterval(buffer, delta, phase);
    }

    Mesh* getMesh() override { return mesh; }
    void setMesh(Mesh* mesh) override { this->mesh = mesh; }
    int getPaddingSize() const override { return rasterizer.getPaddingSize(); }
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }
    RasterizerData& getRastData() { return rasterizerData; }
    RenderState createRenderState() {
        RenderState state;
        saveStateTo(state);
        return state;
    }

    static RenderState createBatchRenderState(
            Scaling scaling,
                const MorphPosition& morphPosition,
                bool lowResCurves = true,
                bool calcDepthDimensions = false) {
        return RenderState(
                true,
                lowResCurves,
                calcDepthDimensions,
                static_cast<int>(scaling),
                morphPosition);
    }

    MorphPosition& getMorphPosition() { return rasterizer.getRequest().morph; }

    Interactor* getInteractor() const { return interactor; }

    void setBatchMode(bool batch) { rasterizer.getRequest().batchMode = batch; }
    void setBlue(float blue) { rasterizer.getRequest().morph.blue.setValueDirect(blue); }
    void setDims(const Dimensions& dims) override { rasterizer.getRequest().dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.getRequest().morph = morph; }
    void setNoiseSeed(int seed) { rasterizer.getRequest().noiseSeed = seed; }
    void setRed(float red) { rasterizer.getRequest().morph.red.setValueDirect(red); }
    void setYellow(float yellow) { rasterizer.getRequest().morph.time.setValueDirect(yellow); }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    static Rasterization::PointScalingMode scalingModeFromRenderState(int scalingType);
    static int renderStateScalingType(Rasterization::PointScalingMode scalingMode);

    void publishSnapshot();
    int primaryViewDimension();

    Mesh* mesh {};
    Rasterization::ComposedMeshWaveformRasterizer rasterizer;
    RasterizerData rasterizerData;

    int layerGroup;
    Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;
