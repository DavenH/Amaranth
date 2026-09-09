#pragma once

#include "Runtime/NodeDspConfiguration.h"

#include <Array/Buffer.h>

#include <memory>

namespace CycleV2 {

struct GraphExecutionPlan;
struct OscillatorRegionPlan;
struct CompiledVoiceContext;
class NodeAudioProcessor;

struct PreparedOscillatorProcessContext {
    const AudioVoiceContext* voice {};
    const SignalPayload* signalBuffers {};
    size_t signalBufferCount {};
    size_t blockFrameCount {};
    size_t blockSampleStart {};
    uint64_t voiceSampleStart {};
    AudioProcessTiming timing;
    int midiNote {};
    float velocity {};
    Buffer<float> pitchEnvelope;
    Buffer<float> left;
    Buffer<float> right;

    const SignalPayload* signalAt(int bufferIndex) const;
};

class PreparedOscillatorRegion {
public:
    virtual ~PreparedOscillatorRegion() = default;
    virtual bool replacesDiagnosticProcessors() const = 0;
    virtual size_t frameRenderCount() const { return 0; }
    virtual void reset() = 0;
    virtual void applyLifecycleEvent(const NoteLifecycleEvent& event) = 0;
    virtual bool process(const PreparedOscillatorProcessContext& context) = 0;
};

std::unique_ptr<PreparedOscillatorRegion> prepareOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const CompiledVoiceContext& context,
        const AudioExecutionSpec& spec,
        int maximumCycleSamples,
        const std::vector<NodeAudioProcessor*>& processors);

bool supportsPreparedOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region);

}
