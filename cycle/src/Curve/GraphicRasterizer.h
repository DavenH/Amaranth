#pragma once

#include <Curve/MeshRasterizer.h>
#include "Rasterization/Facades/GraphicRasterizerFacade.h"
#include "../Updating/DynamicDetailUpdater.h"

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
    GraphicRasterizer(SingletonRepo* repo,
                      Interactor* interactor,
                      const String& name, int layerGroup,
                      bool cyclic, float margin);

    void pullModPositionAndAdjust();
    void restoreStateFrom(MeshRasterizer::RenderState& state) { rasterizer.restoreStateFrom(state); }
    void saveStateTo(MeshRasterizer::RenderState& state) { rasterizer.saveStateTo(state); }

    void calcCrossPoints(Mesh* mesh, float oscPhase) { rasterizer.calcCrossPoints(mesh, oscPhase); }
    void cleanUp() override { rasterizer.cleanUp(); }
    void performUpdate(UpdateType updateType) override { rasterizer.performUpdate(updateType); }
    void reset() override { rasterizer.reset(); }
    void updateRasterizer(UpdateType updateType) override { update(updateType); }

    bool hasEnoughCubesForCrossSection() override { return rasterizer.hasEnoughCubesForCrossSection(); }
    bool isSampleable() override { return rasterizer.isSampleable(); }
    bool isSampleableAt(float x) override { return rasterizer.isSampleableAt(x); }
    bool wrapsVertices() const override { return rasterizer.wrapsVertices(); }

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

    Mesh* getMesh() override { return rasterizer.getMesh(); }
    void setMesh(Mesh* mesh) override { rasterizer.setMesh(mesh); }
    int getPaddingSize() const override { return rasterizer.getPaddingSize(); }
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizer.getRasterizerData(); }
    const RasterizerData& getRasterizerData() const override { return rasterizer.getRasterizerData(); }
    RasterizerData& getRastData() { return rasterizer.getRastData(); }
    MeshRasterizer::RenderState createRenderState() {
        MeshRasterizer::RenderState state;
        saveStateTo(state);
        return state;
    }

    MorphPosition& getMorphPosition() { return rasterizer.getMorphPosition(); }
    MeshRasterizer& legacyRasterizer() { return rasterizer; }

    Interactor* getInteractor() const { return interactor; }

    void setBatchMode(bool batch) { rasterizer.setBatchMode(batch); }
    void setBlue(float blue) { rasterizer.setBlue(blue); }
    void setDims(const Dimensions& dims) override { rasterizer.setDims(dims); }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.setMorphPosition(morph); }
    void setNoiseSeed(int seed) { rasterizer.setNoiseSeed(seed); }
    void setRed(float red) { rasterizer.setRed(red); }
    void setYellow(float yellow) { rasterizer.setYellow(yellow); }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    MeshRasterizer rasterizer;
    int layerGroup;
    Interactor* interactor;
};

typedef GraphicRasterizer TimeRasterizer;
typedef GraphicRasterizer SpectRasterizer;
typedef GraphicRasterizer PhaseRasterizer;
