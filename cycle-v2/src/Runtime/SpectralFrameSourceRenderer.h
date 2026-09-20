#pragma once

#include <Array/Buffer.h>
#include <Audio/CycleDsp/SourceRenderPerformance.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>

#include <memory>

namespace CycleV2 {

class PreparedCycleEnvelopeBank;
struct PreparedOscillatorProcessContext;
struct GraphExecutionPlan;
struct GraphExecutionStep;

struct SpectralFrameSourceRenderRequest {
    int frameSize {};
    int midiNote {};
    int activeHarmonicCount {};
    size_t blockSampleOffset {};
    double voiceSamplePosition {};
    size_t elapsedSamples {};
    bool refreshTimeSource { true };
    const PreparedOscillatorProcessContext* context {};
    const PreparedCycleEnvelopeBank* cycleEnvelopes {};
    Random* random {};
    const CycleDsp::SpectralFrameCapture* capture {};
    CycleDsp::SourceRenderPerformance* performance {};
    Buffer<float> phaseHarmonicScale;
};

class SpectralFrameSourceRenderer {
public:
    virtual ~SpectralFrameSourceRenderer() = default;

    static std::unique_ptr<SpectralFrameSourceRenderer> create(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step,
            int maximumFrameSize);

    virtual void reset() = 0;
    virtual void setVoiceLifecycleSeeds(
            uint32_t timeSeed,
            uint32_t magnitudeSeed,
            uint32_t phaseSeed) = 0;
    virtual void render(
            const SpectralFrameSourceRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) = 0;
    virtual bool isTimeSource() const = 0;
    virtual bool hasActiveSpectralMesh() const = 0;
};

}
