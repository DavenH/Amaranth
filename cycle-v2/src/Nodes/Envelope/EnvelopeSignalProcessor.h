#pragma once

#include <Array/ScopedAlloc.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "../../Runtime/AudioProcessContextUtils.h"
#include "../../Runtime/NodeDspConfiguration.h"
#include "../../Runtime/SmoothedMorphPosition.h"
#include "EnvelopeConfiguration.h"
#include "EnvelopeMeshState.h"
#include "EnvelopePreparationExchange.h"

namespace CycleV2 {

class EnvelopeSignalProcessor {
public:
    struct DynamicDiagnostics {
        uint64_t requests {};
        uint64_t preparations {};
        uint64_t adoptions {};
        uint64_t staleResults {};
    };

    EnvelopeSignalProcessor();

    static std::shared_ptr<const EnvelopeConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters);

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);
    bool serviceNonRealtimePreparation();

    void process(AudioProcessContext& context);
    DynamicDiagnostics dynamicDiagnostics() const;
    double playbackPosition() const {
        return playback.samplePosition(Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex);
    }
    Rasterization::EnvelopePlaybackMode playbackMode() const { return playback.mode(); }

private:
    void requestEffectiveMorph(AudioProcessContext& context);
    void adoptPreparedDynamicEnvelope();
    const EnvelopeConfiguration* preparedConfiguration() const;
    static std::shared_ptr<const EnvelopeConfiguration> prepareDynamicConfiguration(
            const EnvelopeConfiguration& base,
            float red,
            float blue);
    void applyLifecycleEvent(const NoteLifecycleEvent& event);
    void renderSegment(Buffer<float> output, size_t start, size_t count, const AudioProcessTiming& timing);
    void applyAdoptionTransition(Buffer<float> rendered);
    void publishTraversalGrid(SignalPayload& output, const AudioProcessWorkArena* arena);

    static constexpr size_t defaultTraversalColumns = 8;
    static constexpr float morphRequestThreshold = 0.002f;
    static constexpr int morphRequestInterval44k = 64;

    Rasterization::EnvelopePlaybackEngine playback;
    MeshLibrary::EnvProps props;
    bool active {};
    float level { 1.f };
    uint64_t adoptedRevision {};
    uint64_t pendingRevision {};
    std::shared_ptr<const EnvelopeConfiguration> configuration;
    std::shared_ptr<const EnvelopeConfiguration> activeConfiguration;
    LatestEnvelopePreparationRequest preparationRequests;
    PreparedEnvelopeExchange preparedEnvelopes;
    uint64_t noteSerial {};
    float lastRequestedRed { 0.5f };
    float lastRequestedBlue { 0.5f };
    bool hasRequestedMorph {};
    bool morphInitialized {};
    bool adoptionTransitionPending {};
    int samplesSinceMorphRequest {};
    int transitionSamplesRemaining {};
    float lastOutputSample {};
    float transitionOffset {};
    SmoothedMorphPosition smoothedMorph;
    ScopedAlloc<float> transitionMemory { 8192 };
    ScopedAlloc<float> traversalMemory { 2 * defaultTraversalColumns };
};

}
