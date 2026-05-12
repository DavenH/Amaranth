#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Intercept.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Curve/Rasterization/TrilinearMeshRasterizer.h>
#include <Curve/VertCube.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "Rasterization/Policies/Voice/VoicePolicies.h"

class SingletonAccessor;
class CycleState;

class VoiceMeshRasterizer :
        public SingletonAccessor
    ,   public Rasterization::TrilinearMeshRasterizer {
private:
	JUCE_LEAK_DETECTOR(VoiceMeshRasterizer)

public:
	CycleState* state;

	explicit VoiceMeshRasterizer(SingletonRepo* repo);

	/*
	 * Chains the minimum necessary number of points from previous call to head of the subsequent call
	 * This provides 100% continuity between cycles
	 */
	void updateChainedWaveform(float phase);
	void orphanOldVerts();
	void setState(CycleState* state) { this->state = state; }

    void cleanUp();
    void reset() override { cleanUp(); }

    Rasterization::SamplerView sampler() const override {
        return Rasterization::SamplerView(currentWaveform(), currentWaveformIsSampleable());
    }

private:
    Rasterization::WaveformBuffers currentWaveform() const;
    bool currentWaveformIsSampleable() const;
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

    Rasterization::RenderResult chainResult;
    VertCube::ReductionData chainReduction;
    Rasterization::TrilinearMeshSlicer voiceSlicer;
    Cycle::Rasterization::VoicePointPositionPolicy voicePointPositionPolicy;

    int chainPaddingSize { 2 };
    bool chainUnsampleable { true };
    bool chainNeedsResorting {};
    bool chainedOutputActive {};
};
