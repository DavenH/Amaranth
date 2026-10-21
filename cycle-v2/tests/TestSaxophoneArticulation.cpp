#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Curve/Rasterization/EnvelopeMaterialization.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>

#include "Graph/GraphSerializer.h"
#include "Graph/NodeParameterMap.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"

using namespace CycleV2;
using namespace juce;

namespace {

GraphLoadResult expressiveSaxophone() {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/expressive-saxophone.cyclegraph");
    REQUIRE(preset.existsAsFile());
    auto loaded = GraphSerializer().loadJsonString(preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    return loaded;
#else
    FAIL("The authored saxophone fixture requires CYCLE_V2_SOURCE_DIR");
    return {};
#endif
}

std::shared_ptr<const EnvelopeConfiguration> pitchConfiguration(const NodeGraph& graph) {
    const Node* pitch = graph.findNode("pitchEnvelope1");
    REQUIRE(pitch != nullptr);
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(
            pitch->parameters, pitch->model, &graph, pitch->id);
    REQUIRE(configuration != nullptr);
    return configuration;
}

double voiceDuration(const NodeGraph& graph) {
    const Node* voice = graph.findNode("voice");
    REQUIRE(voice != nullptr);
    return CycleDsp::voiceLengthSeconds(NodeParameterMap(*voice).floatValue("voiceLength"));
}

Rasterization::EnvelopePlaybackEngine pitchPlayback() {
    Rasterization::EnvelopePlaybackEngine playback;
    playback.ensureVoiceCount(1);
    playback.setOneSamplePerCycle(true);
    playback.noteOn();
    return playback;
}

constexpr int head = Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex;
constexpr double stepSeconds = .001;

std::vector<double> sampleWrapTimes(
        Rasterization::EnvelopePlaybackEngine& playback,
        const Rasterization::PreparedEnvelopePlaybackView& view,
        double duration,
        int sampleCount) {
    MeshLibrary::EnvProps props;
    props.active = true;
    std::vector<double> wraps;
    for (int sample = 0; sample < sampleCount; ++sample) {
        const double previous = playback.samplePosition(head);
        REQUIRE(playback.renderToBuffer(view, 1, stepSeconds / duration, head, props, 1.f));
        if (playback.samplePosition(head) < previous) {
            wraps.push_back((sample + 1) * stepSeconds);
        }
    }
    return wraps;
}

}

TEST_CASE("Authored saxophone pitch shapes play neutral depth and independently selected vibrato rates",
        "[cycle-v2][envelope][saxophone][articulation]") {
    const auto loaded = expressiveSaxophone();
    const auto configuration = pitchConfiguration(loaded.graph);
    const double duration = voiceDuration(loaded.graph);
    Rasterization::RealtimeEnvelopeMaterializer materializer;
    REQUIRE(materializer.prepare(configuration->realtimePlan));
    for (const auto [red, rate] : std::array<std::pair<float, double>, 2> {{ { 0.f, 4.5 }, { 1.f, 7.5 } }}) {
        INFO("Authored vibrato endpoint rate " << rate);
        REQUIRE(materializer.materialize(red, 0.f));
        auto playback = pitchPlayback();
        const auto neutral = materializer.preparedPlaybackView();
        MeshLibrary::EnvProps props;
        props.active = true;
        for (int sample = 0; sample < 1500; ++sample) {
            REQUIRE(playback.renderToBuffer(neutral, 1, stepSeconds / duration, head, props, 1.f));
            REQUIRE(playback.sustainLevel(head) == Catch::Approx(.5f).margin(1e-7f));
        }

        REQUIRE(materializer.materialize(red, 1.f));
        const auto fullDepth = materializer.preparedPlaybackView();
        const auto span = Rasterization::EnvelopePlaybackEngine::loopSpan(fullDepth);
        REQUIRE(span.start * duration == Catch::Approx(.2).margin(.0001));
        REQUIRE(span.length * duration == Catch::Approx(1. / rate).margin(.0001));
        playback = pitchPlayback();
        float minimum = 1.f;
        float maximum = 0.f;
        float earlyDeviation = 0.f;
        for (int sample = 0; sample < 1500; ++sample) {
            REQUIRE(playback.renderToBuffer(fullDepth, 1, stepSeconds / duration, head, props, 1.f));
            const float value = playback.sustainLevel(head);
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
            if (sample * stepSeconds < .1) {
                earlyDeviation = std::max(earlyDeviation, std::abs(value - .5f));
            }
        }
        REQUIRE(earlyDeviation * 2400.f < 1.f);
        REQUIRE((maximum - .5f) * 2400.f == Catch::Approx(30.f).margin(2.f));
        REQUIRE((.5f - minimum) * 2400.f == Catch::Approx(30.f).margin(2.f));
        REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Looping);

        const auto wraps = sampleWrapTimes(playback, fullDepth, duration, 1500);
        REQUIRE(wraps.size() >= 5);
        for (size_t index = 1; index < wraps.size(); ++index) {
            REQUIRE(wraps[index] - wraps[index - 1] == Catch::Approx(1. / rate).margin(.002));
        }
    }
}

TEST_CASE("Authored saxophone live rate adoption preserves loop phase and delayed-envelope state",
        "[cycle-v2][envelope][saxophone][articulation][live]") {
    const auto loaded = expressiveSaxophone();
    const auto configuration = pitchConfiguration(loaded.graph);
    const double duration = voiceDuration(loaded.graph);
    Rasterization::RealtimeEnvelopeMaterializer materializer;
    REQUIRE(materializer.prepare(configuration->realtimePlan));
    REQUIRE(materializer.materialize(0.f, 1.f));
    auto playback = pitchPlayback();
    const auto slow = materializer.preparedPlaybackView();
    REQUIRE(sampleWrapTimes(playback, slow, duration, 910).size() >= 2);
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Looping);
    const auto previous = Rasterization::EnvelopePlaybackEngine::loopSpan(slow);
    const double phase = (playback.samplePosition(head) - previous.start) / previous.length;
    const float previousValue = playback.sustainLevel(head);
    REQUIRE(phase > .05);
    REQUIRE(phase < .95);

    REQUIRE(materializer.stage(1.f, 1.f));
    materializer.adoptStaged();
    const auto fast = materializer.preparedPlaybackView();
    playback.adopt(previous, fast);
    const auto next = Rasterization::EnvelopePlaybackEngine::loopSpan(fast);
    REQUIRE((playback.samplePosition(head) - next.start) / next.length
            == Catch::Approx(phase).margin(1e-5));
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Looping);
    MeshLibrary::EnvProps props;
    props.active = true;
    REQUIRE(playback.renderToBuffer(fast, 1, stepSeconds / duration, head, props, 1.f));
    REQUIRE(playback.sustainLevel(head) == Catch::Approx(previousValue).margin(2.f / 2400.f));
    const auto wraps = sampleWrapTimes(playback, fast, duration, 800);
    REQUIRE(wraps.size() >= 5);
    for (size_t index = 1; index < wraps.size(); ++index) {
        REQUIRE(wraps[index] - wraps[index - 1] == Catch::Approx(1. / 7.5).margin(.002));
    }
    REQUIRE(playback.noteOff(fast));
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Releasing);
    REQUIRE(materializer.materialize(0.f, 1.f));
    playback.adopt(next, materializer.preparedPlaybackView());
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Releasing);
}
