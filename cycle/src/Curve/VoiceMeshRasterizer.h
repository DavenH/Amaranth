#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Intercept.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Rasterizer.h>
#include <Curve/Rasterization/MeshWaveformRasterizer.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Curve/RasterizerData.h>
#include <Curve/VertCube.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "Rasterization/Policies/Voice/VoicePolicies.h"

class SingletonAccessor;
class CycleState;

class VoiceMeshRasterizer :
        public SingletonAccessor
    ,   public Rasterization::Rasterizer {
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

    void calcCrossPoints(Mesh* mesh, float oscPhase);
    void cleanUp();
    void performUpdate(UpdateType updateType) override;
    void reset() override { cleanUp(); }

    bool doesCalcDepthDimensions() const { return rasterizer.getRequest().calcDepthDimensions; }
    bool doesIntegralSampling() const { return rasterizer.getRequest().integralSampling; }
    bool hasEnoughCubesForCrossSection();
    bool isSampleable() override;
    bool isSampleableAt(float x) override;
    bool wrapsVertices() const override { return rasterizer.getRequest().cyclic; }

    float sampleAt(double angle) override;
    float sampleAt(double angle, int& currentIndex) override;
    float samplePerfectly(double delta, Buffer<float> buffer, double phase);

    template<typename T>
    T sampleWithInterval(Buffer<float> buffer, T delta, T phase) {
        if (!chainedOutputActive) {
            return rasterizer.sampleWithInterval(buffer, delta, phase);
        }

        return Rasterization::WaveformSampler::sampleWithInterval(
                chainResult.waveform,
                buffer,
                delta,
                phase);
    }

    Mesh* getMesh() { return mesh; }
    void setMesh(Mesh* mesh) override { this->mesh = mesh; }
    int getPaddingSize() const override;
    GuideCurveProvider* getGuideCurveProvider() const override { return rasterizer.getGuideCurveProvider(); }
    RasterizerData& getRasterizerData() override { return rasterizerData; }
    const RasterizerData& getRasterizerData() const override { return rasterizerData; }

    MorphPosition& getMorphPosition() { return rasterizer.getRequest().morph; }
    void setCalcDepthDimensions(bool calc) { rasterizer.getRequest().calcDepthDimensions = calc; }
    void setDims(const Dimensions& dims) override { rasterizer.getRequest().dims = dims; }
    void setGuideCurveProvider(GuideCurveProvider* provider) override { rasterizer.setGuideCurveProvider(provider); }
    void setIntegralSampling(bool does) { rasterizer.getRequest().integralSampling = does; }
    void setInterceptPadding(float value) { rasterizer.getRequest().interceptPadding = value; }
    void setMorphPosition(const MorphPosition& morph) { rasterizer.getRequest().morph = morph; }
    void setNoiseSeed(int seed) { rasterizer.getRequest().noiseSeed = seed; }
    void setWrapsEnds(bool wraps) { rasterizer.getRequest().cyclic = wraps; }
    void updateOffsetSeeds(int layerSize, int tableSize) { rasterizer.updateOffsetSeeds(layerSize, tableSize); }

private:
    Rasterization::WaveformBuffers currentWaveform() const;
    void bakeChainedWaveform();
    void cleanChainedOutput();
    Rasterization::RenderResult renderVoiceSlice(float oscPhase);
    void appendVoiceCubeIntercept(
            VertCube* cube,
            float voiceTime,
            float oscPhase,
            Rasterization::GuideCurveApplier& applyGuide,
            std::vector<Intercept>& intercepts);
    void markChainedWaveformUnsampleable();
    void publishSnapshot();
    void updateChainBuffers(int size);
    void restrictIntercepts(std::vector<Intercept>& intercepts);

    Mesh* mesh {};
    Rasterization::MeshWaveformRasterizer rasterizer;
    Rasterization::RenderResult chainResult;
    RasterizerData rasterizerData;
    VertCube::ReductionData chainReduction;
    Rasterization::TrilinearMeshSlicer voiceSlicer;
    Cycle::Rasterization::VoicePointPositionPolicy voicePointPositionPolicy;

    int chainPaddingSize { 2 };
    bool chainUnsampleable { true };
    bool chainNeedsResorting {};
    bool chainedOutputActive {};
};
