#pragma once

#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>
#include "Rasterization/Facades/VoiceRasterizerFacade.h"

class SingletonAccessor;
class CycleState;

class VoiceMeshRasterizer :
        public SingletonAccessor
    ,   public Rasterization::GuideCurveBindableRasterizer
    ,   public Rasterization::MeshBindableRasterizer
    ,   public Rasterization::RasterizerSampler
    ,   public Rasterization::RasterizerSnapshotProvider
    ,   public Rasterization::RasterizerUpdateTarget
    ,   public Rasterization::RasterizerVertexDomain {
private:
	JUCE_LEAK_DETECTOR(VoiceMeshRasterizer)

public:
	CycleState* state;

	explicit VoiceMeshRasterizer(SingletonRepo* repo);

	/*
	 * Chains the minimum necessary number of points from previous call to head of the subsequent call
	 * This provides 100% continuity between cycles
	 */
	void calcCrossPointsChaining(float phase);
	void orphanOldVerts();
	void setState(CycleState* state) { this->state = state; }

    void calcCrossPoints(Mesh* mesh, float oscPhase) { rasterizer.calcCrossPoints(mesh, oscPhase); }
    void cleanUp() override { rasterizer.cleanUp(); }
    void performUpdate(UpdateType updateType) override { rasterizer.performUpdate(updateType); }
    void reset() override { rasterizer.reset(); }
    void updateRasterizer(UpdateType updateType) override { rasterizer.updateRasterizer(updateType); }

    bool doesCalcDepthDimensions() const { return rasterizer.doesCalcDepthDimensions(); }
    bool doesIntegralSampling() const { return rasterizer.doesIntegralSampling(); }
    bool hasEnoughCubesForCrossSection() override { return rasterizer.hasEnoughCubesForCrossSection(); }
    bool isSampleable() override { return rasterizer.isSampleable(); }
    bool isSampleableAt(float x) override { return rasterizer.isSampleableAt(x); }
    bool wrapsVertices() const override { return rasterizer.wrapsVertices(); }

    float sampleAt(double angle) override { return rasterizer.sampleAt(angle); }
    float sampleAt(double angle, int& currentIndex) override { return rasterizer.sampleAt(angle, currentIndex); }
    float samplePerfectly(double delta, Buffer<float> buffer, double phase) override {
        return rasterizer.samplePerfectly(delta, buffer, phase);
    }

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

    MorphPosition& getMorphPosition() { return rasterizer.getMorphPosition(); }
    void setCalcDepthDimensions(bool calc) { rasterizer.setCalcDepthDimensions(calc); }
    void setDims(const Dimensions& dims) override { rasterizer.setDims(dims); }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setIntegralSampling(bool does) { rasterizer.setIntegralSampling(does); }
    void setInterceptPadding(float value) { rasterizer.setInterceptPadding(value); }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.setMorphPosition(morph); }
    void setNoiseSeed(int seed) { rasterizer.setNoiseSeed(seed); }
    void setWrapsEnds(bool wraps) { rasterizer.setWrapsEnds(wraps); }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    MeshRasterizer rasterizer;
};
