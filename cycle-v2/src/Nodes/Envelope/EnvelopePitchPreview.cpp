#include <Curve/GuideCurveProvider.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>

#include "Nodes/Envelope/EnvelopeConfiguration.h"
#include "Nodes/Envelope/EnvelopePitchPreview.h"

namespace CycleV2 {

std::vector<std::vector<float>> EnvelopePitchPreview::renderLanes(
        const std::shared_ptr<const EnvelopeConfiguration>& configuration,
        int laneCount,
        int sampleCount,
        uint32_t seed) {
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
            || !materializer.materialize(
                    configuration->redMorph,
                    configuration->blueMorph)) {
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
    const double increment = sampleCount > 1
            ? 1.0 / (double) (sampleCount - 1)
            : 1.0;
    for (int sample = 0; sample < sampleCount; ++sample) {
        for (int lane = 0; lane < laneCount; ++lane) {
            const int voiceIndex = Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex
                    + lane;
            playback.renderToBuffer(
                    prepared,
                    1,
                    increment,
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
