#include <Curve/GuideCurveProvider.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>

#include "Nodes/Envelope/EnvelopeConfiguration.h"
#include "Nodes/Envelope/EnvelopePitchPreview.h"
#include "Runtime/PreviewTraversal.h"

namespace CycleV2 {

std::vector<std::vector<float>> EnvelopePitchPreview::renderLanes(
        const std::shared_ptr<const EnvelopeConfiguration>& configuration,
        int laneCount,
        int sampleCount,
        uint32_t seed,
        double releaseDuration,
        double noteOffTime) {
    if (configuration == nullptr || laneCount < 1 || sampleCount < 1) {
        return {};
    }

    std::vector<std::vector<float>> result(
            static_cast<size_t>(laneCount),
            std::vector<float>(
                    static_cast<size_t>(sampleCount),
                    configuration->neutralValue));
    if (!configuration->enabled) {
        return result;
    }

    Rasterization::RealtimeEnvelopeMaterializer materializer;
    const auto& plan = configuration->cycleGuideRequired
            ? configuration->cycleRealtimePlan
            : configuration->realtimePlan;
    if (!materializer.prepare(plan)
            || !materializer.materialize(configuration->morph)) {
        return {};
    }

    Rasterization::EnvelopePlaybackEngine playback;
    playback.ensureVoiceCount(laneCount);
    playback.setOneSamplePerCycle(true);
    playback.validate(materializer.preparedPlaybackView());
    playback.deriveVoiceOffsets(
            GuideCurveProvider::tableSize,
            Rasterization::GuideCurveSeed::visualization(seed));
    playback.noteOn();
    const auto prepared = materializer.preparedPlaybackView();

    MeshLibrary::EnvProps props;
    props.active = true;
    props.logarithmic = configuration->logarithmic;
    const PreviewTraversal traversal((size_t) sampleCount, releaseDuration, noteOffTime);
    for (int sample = 0; sample < sampleCount; ++sample) {
        if (traversal.beginsRelease((size_t) sample)) {
            playback.noteOff(prepared);
        }
        for (int lane = 0; lane < laneCount; ++lane) {
            const int voiceIndex = Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex
                    + lane;
            playback.renderToBuffer(
                    prepared,
                    1,
                    traversal.increment((size_t) sample),
                    voiceIndex,
                    props,
                    1.f);
            result[static_cast<size_t>(lane)][static_cast<size_t>(sample)]
                    = playback.sustainLevel(voiceIndex);
        }
    }
    return result;
}

}
