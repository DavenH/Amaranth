#include "Runtime/GraphAudioExecutor.h"

#include "Nodes/Envelope/CycleEnvelopePlaybackSource.h"
#include "Runtime/ProbeExecutionScope.h"

namespace CycleV2 {

double GraphAudioExecutor::measurePreviewRelease(
        const GraphExecutionPlan& plan,
        const AudioVoiceContext& voice,
        AudioProcessTiming timing) const {
    std::vector<int> envelopes;
    for (size_t index = 0; index < plan.steps.size(); ++index) {
        if (plan.steps[index].audioRole == AudioModuleRole::Envelope) {
            envelopes.push_back((int) index);
        }
    }
    if (envelopes.empty()) {
        return 0.;
    }

    // Prepare the same routed note-on shapes as audio, without rendering any grids.
    if (envelopeTimingExecutor == nullptr) {
        envelopeTimingExecutor = std::make_unique<GraphAudioExecutor>();
    }
    const auto activeSteps = probeExecutionSteps(plan, envelopes);
    AudioVoiceContext noteOn = voice;
    noteOn.events = { { NoteLifecycleType::NoteOn, 0, voice.voiceIndex } };
    envelopeTimingExecutor->processInternal(plan, 1, timing, noteOn,
            CompleteDiagnosticExecution { 1, &activeSteps, -1, nullptr, true });
    const auto prepared = envelopeTimingExecutor->preparedVoices.find(voice.voiceIndex);
    if (prepared == envelopeTimingExecutor->preparedVoices.end()) {
        return 0.;
    }

    double duration = 0.;
    for (const int index : envelopes) {
        const auto* processor = prepared->second.processors[(size_t) index];
        const auto* source = processor != nullptr ? processor->cycleEnvelopePlaybackSource() : nullptr;
        if (source == nullptr || source->cycleEnvelopeConfiguration() == nullptr
                || !source->cycleEnvelopeConfiguration()->enabled) {
            continue;
        }
        duration = jmax(duration, Rasterization::EnvelopePlaybackEngine::releaseDuration(
                source->cycleEnvelopePlaybackView()));
    }
    return duration;
}

}
