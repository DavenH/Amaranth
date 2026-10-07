#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Nodes/Trimesh/Dsp/TrimeshPhaseAccumulator.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphSerializer.h"
#include "Graph/NodeParameterMap.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/SpectralFrameSourceRenderer.h"

#include "NodeGraphTestAccess.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace CycleV2;

TEST_CASE("Trimesh phase velocity accumulates by elapsed time per bin and resets",
        "[cycle-v2][trimesh][phase-velocity]") {
    TrimeshPhaseAccumulator accumulator;
    accumulator.prepare(3);
    std::array<float, 3> first { 0.f, MathConstants<float>::twoPi,
            -MathConstants<float>::twoPi };
    accumulator.integrate({ first.data(), 3 }, 0.0);
    REQUIRE(first[1] == Catch::Approx(0.f));

    std::array<float, 3> second { 0.f, MathConstants<float>::twoPi,
            -MathConstants<float>::twoPi };
    accumulator.integrate({ second.data(), 3 }, 0.25);
    REQUIRE(second[0] == Catch::Approx(0.f));
    REQUIRE(second[1] == Catch::Approx(MathConstants<float>::halfPi));
    REQUIRE(second[2] == Catch::Approx(-MathConstants<float>::halfPi));

    std::array<float, 3> third { 0.f, MathConstants<float>::twoPi,
            -MathConstants<float>::twoPi };
    accumulator.integrate({ third.data(), 3 }, 0.25);
    REQUIRE(third[1] == Catch::Approx(MathConstants<float>::pi));
    REQUIRE(third[2] == Catch::Approx(-MathConstants<float>::pi));

    accumulator.reset();
    std::array<float, 3> newNote { 0.f, MathConstants<float>::twoPi,
            -MathConstants<float>::twoPi };
    accumulator.integrate({ newNote.data(), 3 }, 0.25);
    REQUIRE(newNote[1] == Catch::Approx(MathConstants<float>::halfPi));
}

TEST_CASE("Trimesh phase velocity is independent of frame cadence",
        "[cycle-v2][trimesh][phase-velocity]") {
    TrimeshPhaseAccumulator fast;
    TrimeshPhaseAccumulator slow;
    fast.prepare(1);
    slow.prepare(1);

    std::array<float, 1> fastValue { MathConstants<float>::twoPi };
    for (int frame = 0; frame < 4; ++frame) {
        fastValue[0] = MathConstants<float>::twoPi;
        fast.integrate({ fastValue.data(), 1 }, 0.125);
    }
    std::array<float, 1> slowValue { MathConstants<float>::twoPi };
    slow.integrate({ slowValue.data(), 1 }, 0.5);
    REQUIRE(fastValue[0] == Catch::Approx(slowValue[0]));

    slowValue[0] = MathConstants<float>::twoPi;
    slow.integrate({ slowValue.data(), 1 }, 1.0);
    REQUIRE(std::abs(slowValue[0]) == Catch::Approx(MathConstants<float>::pi));
}

TEST_CASE("Prepared Trimesh phase velocity reaches the spectral graph and resets",
        "[cycle-v2][trimesh][phase-velocity][runtime]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/bright-lead-6-c.cyclegraph");
    REQUIRE(preset.existsAsFile());
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    NodeGraph graph = loaded.graph;
    Node* phaseNode = NodeGraphTestAccess::findNodeForEditing(
            graph, "phaseLayer1");
    REQUIRE(phaseNode != nullptr);
    auto phaseMode = std::find_if(
            phaseNode->parameters.begin(),
            phaseNode->parameters.end(),
            [](const NodeParameter& parameter) {
                return parameter.id == "phaseMode";
            });
    REQUIRE(phaseMode != phaseNode->parameters.end());
    REQUIRE(phaseMode->value == "absolute");
    phaseMode->value = "velocity";
    const auto roundTrip = GraphSerializer().loadJsonString(
            GraphSerializer().toJsonString(graph));
    REQUIRE(roundTrip.succeeded());
    const Node* savedPhase = roundTrip.graph.findNode("phaseLayer1");
    REQUIRE(savedPhase != nullptr);
    REQUIRE(NodeParameterMap(*savedPhase).stringValue("phaseMode") == "velocity");
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const GraphExecutionStep* phaseStep = nullptr;
    for (const auto& step : compiled.plan.steps) {
        if (step.nodeId == "phaseLayer1") {
            phaseStep = &step;
            break;
        }
    }
    REQUIRE(phaseStep != nullptr);
    auto source = SpectralFrameSourceRenderer::create(
            compiled.plan, *phaseStep, 2048);
    REQUIRE(source != nullptr);

    constexpr int binCount = 1025;
    std::array<float, binCount> left {};
    std::array<float, binCount> right {};
    std::array<float, binCount - 1> harmonicScale {};
    harmonicScale.fill(1.f);
    Random random(12345);
    CycleDsp::SpectralFrameCapture capture(nullptr, 0, 0, 60);
    SpectralFrameSourceRenderRequest request;
    request.frameSize = 2048;
    request.midiNote = 60;
    request.activeHarmonicCount = 32;
    request.random = &random;
    request.capture = &capture;
    request.phaseHarmonicScale = { harmonicScale.data(), binCount - 1 };

    source->render(request, { left.data(), binCount }, { right.data(), binCount });
    REQUIRE(std::all_of(left.begin(), left.end(), [](float value) {
        return value == 0.f;
    }));

    request.voiceSamplePosition = 11025.0;
    source->render(request, { left.data(), binCount }, { right.data(), binCount });
    REQUIRE(std::any_of(left.begin() + 1, left.end(), [](float value) {
        return value != 0.f;
    }));
    REQUIRE(std::equal(left.begin(), left.end(), right.begin()));

    source->reset();
    request.voiceSamplePosition = 0.0;
    source->render(request, { left.data(), binCount }, { right.data(), binCount });
    REQUIRE(std::all_of(left.begin(), left.end(), [](float value) {
        return value == 0.f;
    }));

    const auto absolutePlan = GraphCompiler().compile(loaded.graph);
    REQUIRE(absolutePlan.succeeded());
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 256;
    spec.sampleRate = 44100.0;
    GraphAudioExecutor velocityExecutor;
    GraphAudioExecutor absoluteExecutor;
    velocityExecutor.prepareExecution(compiled.plan, spec);
    absoluteExecutor.prepareExecution(absolutePlan.plan, spec);
    AudioVoiceContext voice;
    voice.controls.noteNumber = 60;
    voice.controls.velocity = 1.f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    double audioDifference = 0.0;
    for (int block = 0; block < 16; ++block) {
        const auto velocityOutput = velocityExecutor.processRealtime(
                compiled.plan, 256, {}, voice);
        const auto absoluteOutput = absoluteExecutor.processRealtime(
                absolutePlan.plan, 256, {}, voice);
        REQUIRE(velocityOutput.isValid());
        REQUIRE(absoluteOutput.isValid());
        const auto& velocitySamples = velocityOutput.payload->block.samples;
        const auto& absoluteSamples = absoluteOutput.payload->block.samples;
        REQUIRE(velocitySamples.size() == absoluteSamples.size());
        for (size_t sample = 0; sample < velocitySamples.size(); ++sample) {
            audioDifference += std::abs(
                    velocitySamples[sample] - absoluteSamples[sample]);
        }
        voice.events.clear();
    }
    REQUIRE(audioDifference > 0.01);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("A Phase Velocity Spy shows accumulated phase across traversal time",
        "[cycle-v2][trimesh][phase-velocity][spy]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/bright-lead-6-c.cyclegraph");
    const auto loaded = GraphSerializer().loadJsonString(preset.loadFileAsString());
    REQUIRE(loaded.succeeded());

    NodeGraph velocityGraph = loaded.graph;
    Node* phase = NodeGraphTestAccess::findNodeForEditing(
            velocityGraph, "phaseLayer1");
    REQUIRE(phase != nullptr);
    auto mode = std::find_if(phase->parameters.begin(), phase->parameters.end(),
            [](const NodeParameter& parameter) {
                return parameter.id == "phaseMode";
            });
    REQUIRE(mode != phase->parameters.end());
    mode->value = "velocity";
    velocityGraph.addSignalProbe({
            "phaseVelocitySpy", "phaseLayer1", "out", "phaseOp1", "right",
            "Phase Velocity", 0.5f, 0
    });

    NodeGraph absoluteGraph = loaded.graph;
    absoluteGraph.addSignalProbe({
            "phaseVelocitySpy", "phaseLayer1", "out", "phaseOp1", "right",
            "Phase Velocity", 0.5f, 0
    });
    const auto velocityPlan = GraphCompiler().compile(velocityGraph);
    const auto absolutePlan = GraphCompiler().compile(absoluteGraph);
    REQUIRE(velocityPlan.succeeded());
    REQUIRE(absolutePlan.succeeded());

    constexpr size_t frameCount = 512;
    constexpr size_t columnCount = 16;
    const auto velocityAudio = GraphAudioExecutor().process(
            velocityGraph, velocityPlan.plan, frameCount, {}, {}, columnCount);
    const auto absoluteAudio = GraphAudioExecutor().process(
            absoluteGraph, absolutePlan.plan, frameCount, {}, {}, columnCount);
    const auto velocityPreview = GraphPreviewExecutor().render(
            velocityPlan.plan, velocityAudio,
            velocityGraph.getSignalProbes(), frameCount);
    const auto absolutePreview = GraphPreviewExecutor().render(
            absolutePlan.plan, absoluteAudio,
            absoluteGraph.getSignalProbes(), frameCount);
    const auto spyIt = std::find_if(
            velocityPreview.probes.begin(), velocityPreview.probes.end(),
            [](const auto& preview) {
                return preview.probeId == "phaseVelocitySpy";
            });
    const auto rateIt = std::find_if(
            absolutePreview.probes.begin(), absolutePreview.probes.end(),
            [](const auto& preview) {
                return preview.probeId == "phaseVelocitySpy";
            });
    REQUIRE(spyIt != velocityPreview.probes.end());
    REQUIRE(rateIt != absolutePreview.probes.end());
    const auto& spy = *spyIt;
    const auto& rate = *rateIt;
    REQUIRE(spy.connected);
    REQUIRE(spy.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(spy.gridColumns == columnCount);
    REQUIRE(spy.gridRows == rate.gridRows);

    const float columnSeconds = (float) frameCount / 44100.f
            / (float) (columnCount - 1);
    bool moved = false;
    for (size_t row = 0; row < spy.gridRows; ++row) {
        float expected = 0.f;
        REQUIRE(spy.values[row] == Catch::Approx(0.f).margin(1.0e-6f));
        for (size_t column = 1; column < columnCount; ++column) {
            const size_t index = column * spy.gridRows + row;
            expected = std::remainder(
                    expected + rate.values[index] * columnSeconds,
                    MathConstants<float>::twoPi);
            REQUIRE(spy.values[index] == Catch::Approx(expected).margin(1.0e-4f));
            moved |= std::abs(spy.values[index]) > 1.0e-4f;
        }
    }
    REQUIRE(moved);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
