#pragma once

#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/VoiceDeclick.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/EnvelopeMaterialization.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "Nodes/Envelope/CycleEnvelopePlaybackSource.h"
#include "Nodes/Envelope/EnvelopeConfiguration.h"
#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "Runtime/AudioProcessContextUtils.h"
#include "Runtime/NodeDspConfiguration.h"

namespace CycleV2 {

class EnvelopeSignalProcessor : public CycleEnvelopePlaybackSource {
public:
    EnvelopeSignalProcessor();

    static std::shared_ptr<const EnvelopeConfiguration> buildConfiguration(
            const std::vector<NodeParameter>& parameters,
            const NodeModelStatePtr& model = {});

    void prepareExecution(const AudioExecutionSpec& spec);
    void adoptConfiguration(const PublishedNodeConfiguration& published);

    void process(AudioProcessContext& context);
    const Rasterization::RealtimeEnvelopeMaterializationDiagnostics&
    realtimePreparationDiagnostics() const {
        return materializer.diagnostics();
    }
    bool isActive() const { return active; }
    const EnvelopeConfiguration* cycleEnvelopeConfiguration() const override {
        return preparedConfiguration();
    }
    Rasterization::PreparedEnvelopePlaybackView cycleEnvelopePlaybackView() const override {
        return preparedPlaybackView();
    }
    double playbackPosition() const {
        return playback.samplePosition(Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex);
    }
    Rasterization::EnvelopePlaybackMode playbackMode() const { return playback.mode(); }

private:
    const EnvelopeConfiguration* preparedConfiguration() const;
    Rasterization::PreparedEnvelopePlaybackView preparedPlaybackView() const;
    bool prepareNoteEnvelope(const AudioProcessContext& context, size_t sampleOffset);
    void applyLifecycleEvent(
            const NoteLifecycleEvent& event,
            const AudioProcessContext& context,
            size_t sampleOffset);
    void renderSegment(Buffer<float> output, size_t start, size_t count, double normalizedTimeIncrement);
    void renderNeutralSegment(Buffer<float> output, size_t start, size_t count);
    void applyAttackDeclick(Buffer<float> rendered);
    void renderReleaseDeclick(Buffer<float> rendered);
    void publishTraversalGrid(SignalPayload& output, const AudioProcessWorkArena* arena);

    static constexpr size_t defaultTraversalColumns = 8;
    Rasterization::EnvelopePlaybackEngine playback;
    Rasterization::RealtimeEnvelopeMaterializer materializer;
    MeshLibrary::EnvProps props;
    bool active {};
    bool fadingIn {};
    bool fadingOut {};
    int attackSamplePosition {};
    int releaseSamplePosition {};
    float level { 1.f };
    uint64_t adoptedRevision {};
    uint64_t pendingRevision {};
    std::shared_ptr<const EnvelopeConfiguration> configuration;
    std::shared_ptr<const EnvelopeConfiguration> activeConfiguration;
    ScopedAlloc<float> traversalMemory { 2 * defaultTraversalColumns };
    ScopedAlloc<float> attackDeclick;
    ScopedAlloc<float> releaseDeclick;
};

}
