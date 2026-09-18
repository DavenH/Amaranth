#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <Curve/GuideCurveProvider.h>

#include "Graph/GraphSerializer.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Runtime/NodeAudioProcessor.h"
#include "Runtime/PreparedCycleEnvelopeBank.h"

using namespace CycleV2;
using namespace juce;

namespace {

class EnvelopeProcessorAdapter final : public NodeAudioProcessor {
public:
    explicit EnvelopeProcessorAdapter(EnvelopeSignalProcessor& processor) : processor(processor) {}

    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }
    void process(AudioProcessContext&) override {}
    const CycleEnvelopePlaybackSource* cycleEnvelopePlaybackSource() const override {
        return &processor;
    }

private:
    EnvelopeSignalProcessor& processor;
};

void prepareProcessor(
        EnvelopeSignalProcessor& processor,
        const String& nodeId,
        const std::shared_ptr<const EnvelopeConfiguration>& configuration) {
    processor.adoptConfiguration({ 1, nodeId, configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 8;
    spec.sampleRate = 44100.0;
    processor.prepareExecution(spec);

    AudioProcessContext noteOn;
    noteOn.frameCount = 8;
    noteOn.timing.sampleRate = spec.sampleRate;
    noteOn.outputPorts = {
            { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono }
    };
    noteOn.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    processor.process(noteOn);
}

std::vector<std::array<float, 3>> renderPitchLanes(
        PreparedCycleEnvelopeBank& bank,
        uint32_t seed) {
    bank.reset();
    bank.applyLifecycleEvent({ NoteLifecycleType::NoteOn, 0, 0 });
    bank.setVoiceLifecycleSeed(seed);

    std::vector<std::array<float, 3>> values;
    for (int cycle = 0; cycle < 512; ++cycle) {
        std::array<float, 3> laneValues {};
        for (int lane = 0; lane < 3; ++lane) {
            bank.advanceLane(lane, 1, 0.01);
            laneValues[(size_t) lane] = bank.pitchValue(lane);
        }
        values.push_back(laneValues);
    }
    return values;
}

}

TEST_CASE("Pitch Envelope Guide phase is distinct per Unison lane and stable per note seed",
        "[cycle-v2][envelope][guide][unison][runtime]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/brass-section.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const Node* pitch = loaded.graph.findNode("pitchEnvelope1");
    REQUIRE(pitch != nullptr);
    auto configuration = EnvelopeSignalProcessor::buildConfiguration(
            pitch->parameters, pitch->model, &loaded.graph, pitch->id);
    REQUIRE(configuration != nullptr);
    REQUIRE(configuration->rasterizer->preparedPlaybackView().display.guideCurveRegions.empty());

    EnvelopeSignalProcessor liveProcessor;
    prepareProcessor(liveProcessor, pitch->id, configuration);
    REQUIRE_FALSE(liveProcessor.cycleEnvelopePlaybackView()
            .display.guideCurveRegions.empty());

    EnvelopeProcessorAdapter source(liveProcessor);
    GraphExecutionPlan plan;
    plan.dependencyIndex.stepIndexById[pitch->id] = 0;
    std::vector<NodeAudioProcessor*> processors { &source };
    PreparedCycleEnvelopeBank bank;
    REQUIRE(bank.prepare(plan, {}, processors, 3, pitch->id));

    const auto first = renderPitchLanes(bank, 101u);
    const auto repeated = renderPitchLanes(bank, 101u);
    const auto nextNote = renderPitchLanes(bank, 202u);
    REQUIRE(first == repeated);
    REQUIRE(first != nextNote);

    bool lanesDiffer = false;
    for (const auto& cycle : first) {
        lanesDiffer |= cycle[0] != cycle[1] || cycle[1] != cycle[2];
    }
    REQUIRE(lanesDiffer);

    EnvRasterizer legacy(
            configuration->guideCurveProvider.get(), "BrassPitchComparison");
    legacy.setMesh(configuration->mesh.get());
    legacy.setMorphPosition({ 0.f, configuration->redMorph, configuration->blueMorph });
    legacy.setLowresCurves(configuration->lowResolution);
    legacy.setCalcDepthDimensions(false);
    legacy.setWantOneSamplePerCycle(true);
    legacy.renderWaveformOnly(configuration->mesh.get(), 0.f);
    legacy.ensureParamSize(3);
    Random lifecycleRandom(101);
    legacy.updateOffsetSeeds(
            1,
            GuideCurveProvider::tableSize,
            Rasterization::GuideCurveSeed::voiceLifecycle(
                    (uint32_t) lifecycleRandom.nextInt()));
    legacy.setNoteOn();
    MeshLibrary::EnvProps props;
    props.active = true;
    bool legacyActive = true;
    std::vector<std::array<float, 3>> legacyValues;
    legacyValues.reserve(first.size());
    for (size_t index = 0; index < first.size(); ++index) {
        std::array<float, 3> laneValues {};
        for (int lane = 0; lane < 3; ++lane) {
            legacyActive &= legacy.renderToBuffer(
                    1,
                    0.01,
                    EnvRasterizer::headUnisonIndex + lane,
                    props,
                    1.f);
            laneValues[(size_t) lane] = legacy.getSustainLevel(
                    EnvRasterizer::headUnisonIndex + lane);
        }
        legacyValues.push_back(laneValues);
    }
    REQUIRE(legacyActive);
    REQUIRE(first == legacyValues);

    NodeGraph plainGraph = loaded.graph;
    REQUIRE(plainGraph.removeGuideAssignment(
            pitch->id, { 1, GuideCurveField::Time }));
    auto plainConfiguration = EnvelopeSignalProcessor::buildConfiguration(
            pitch->parameters, pitch->model, &plainGraph, pitch->id);
    REQUIRE(plainConfiguration != nullptr);
    EnvelopeSignalProcessor plainProcessor;
    prepareProcessor(plainProcessor, pitch->id, plainConfiguration);
    EnvelopeProcessorAdapter plainSource(plainProcessor);
    std::vector<NodeAudioProcessor*> plainProcessors { &plainSource };
    PreparedCycleEnvelopeBank plainBank;
    REQUIRE(plainBank.prepare(plan, {}, plainProcessors, 3, pitch->id));
    const auto plain = renderPitchLanes(plainBank, 101u);
    REQUIRE(std::all_of(plain.begin(), plain.end(), [](const auto& cycle) {
        return cycle[0] == cycle[1] && cycle[1] == cycle[2];
    }));
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}
