#pragma once

#include "../../Runtime/AudioProcessContextUtils.h"
#include "../../Runtime/NodeDspConfiguration.h"
#include "../../Runtime/SmoothedMorphPosition.h"
#include "EnvelopeMeshState.h"

#include <Array/ScopedAlloc.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>

#include <array>
#include <atomic>

namespace CycleV2 {

struct EnvelopeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }

    std::shared_ptr<EnvelopeMesh> mesh;
    std::shared_ptr<EnvRasterizer> rasterizer;
    float level { 1.f };
    float redMorph { 0.5f };
    float blueMorph { 0.5f };
    bool logarithmic {};
    bool dynamicWhileLive {};
    String meshSnapshot;
};

class EnvelopeSignalProcessor {
public:
    struct DynamicDiagnostics {
        uint64_t requests {};
        uint64_t preparations {};
        uint64_t adoptions {};
        uint64_t staleResults {};
    };

    EnvelopeSignalProcessor();
    ~EnvelopeSignalProcessor();

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
    struct DynamicRequest {
        uint64_t generation {};
        uint64_t noteSerial {};
        float red {};
        float blue {};
    };

    struct PreparedSlot {
        std::shared_ptr<const EnvelopeConfiguration> configuration;
        uint64_t generation {};
        uint64_t noteSerial {};
    };

    bool syncModel(const std::vector<NodeParameter>& parameters);
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

    EnvelopeMesh mesh;
    EnvRasterizer rasterizer;
    Rasterization::EnvelopePlaybackEngine playback;
    MeshLibrary::EnvProps props;
    String snapshotState;
    float redMorph { -1.f };
    float blueMorph { -1.f };
    bool active {};
    bool modelReady {};
    float level { 1.f };
    uint64_t adoptedRevision {};
    uint64_t pendingRevision {};
    std::shared_ptr<const EnvelopeConfiguration> configuration;
    std::shared_ptr<const EnvelopeConfiguration> activeConfiguration;
    std::array<PreparedSlot, 3> preparedSlots;
    std::atomic<uint64_t> requestedGeneration {};
    std::atomic<uint64_t> preparedGeneration {};
    std::atomic<uint64_t> adoptedDynamicGeneration {};
    std::atomic<uint64_t> requestNoteSerial {};
    std::atomic<float> requestedRed { 0.5f };
    std::atomic<float> requestedBlue { 0.5f };
    std::atomic<int> publishedSlot { -1 };
    std::atomic<int> activeSlot { -1 };
    std::atomic<uint64_t> requestCount {};
    std::atomic<uint64_t> preparationCount {};
    std::atomic<uint64_t> adoptionCount {};
    std::atomic<uint64_t> staleResultCount {};
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
