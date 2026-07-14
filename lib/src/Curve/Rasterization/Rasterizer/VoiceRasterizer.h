#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/Policies/Voice/VoicePolicies.h>
#include <Curve/Rasterization/RenderResult.h>
#include <Curve/Rasterization/VoiceCycleState.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Curve/Mesh/VertCube.h>

namespace Rasterization {

class VoiceRasterizer : public TrilinearMeshRasterizer {
private:
    JUCE_LEAK_DETECTOR(VoiceRasterizer)

public:
    static constexpr float defaultMinimumLineLength = 0.001f;

    explicit VoiceRasterizer(float minimumLineLength = defaultMinimumLineLength);

    /*
     * Chains the minimum necessary number of points from the previous call to
     * the head of the subsequent call, preserving continuity between cycles.
     */
    void updateChainedWaveform(float phase);
    void orphanOldVerts();
    void setState(VoiceCycleState* state) { this->state = state; }

    void cleanUp();
    void reset() { cleanUp(); }

    Rasterization::SamplerView sampler() const override {
        return SamplerView(currentWaveform(), currentWaveformIsSampleable());
    }

private:
    WaveformBuffers currentWaveform() const;
    bool currentWaveformIsSampleable() const;
    void bakeChainedWaveform();
    void cleanChainedOutput();
    RenderResult renderVoiceSlice(float oscPhase);
    void appendVoiceCubeIntercept(
            VertCube* cube,
            float voiceTime,
            float oscPhase,
            GuideCurveApplier& applyGuide,
            std::vector<Intercept>& intercepts);
    void markChainedWaveformUnsampleable();
    void publishSnapshot();
    void updateChainBuffers(int size);
    void restrictIntercepts(std::vector<Intercept>& intercepts);

    VoiceCycleState* state {};
    RenderResult chainResult;
    VertCube::ReductionData chainReduction;
    TrilinearMeshSlicer voiceSlicer;
    VoicePointPositionPolicy voicePointPositionPolicy;

    float initialAdvancement;
    int chainPaddingSize { 2 };
    bool chainUnsampleable { true };
    bool chainNeedsResorting {};
    bool chainedOutputActive {};
};

}
