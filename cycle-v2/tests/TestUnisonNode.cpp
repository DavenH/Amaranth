#include <cmath>
#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Effects/EffectPreviewRenderer.h"
#include "../src/Nodes/Unison/UnisonNode.h"
#include "../src/Runtime/NodeDspConfiguration.h"

using namespace CycleV2;

namespace {

void setParameter(Node& node, const String& id, const String& value) {
    for (auto& parameter : node.parameters) {
        if (parameter.id == id) {
            parameter.value = value;
            return;
        }
    }
    FAIL("Missing parameter " << id);
}

double travelledCycles(const UnisonPreviewPath& path) {
    double distance = 0.0;
    for (const auto& segment : path.segments) {
        distance += std::abs(segment.endPhaseCycles - segment.startPhaseCycles);
    }
    return distance;
}

}

TEST_CASE("Unison node publishes the Cycle 1 group configuration",
        "[cycle-v2][unison][graph][dsp]") {
    Node node = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    setParameter(node, "order", "3");
    setParameter(node, "width", "70");
    setParameter(node, "panSpread", "1");
    setParameter(node, "phase", "0.5");
    setParameter(node, "jitter", "0.8");

    REQUIRE(node.inputs.empty());
    REQUIRE(node.outputs.size() == 1);
    REQUIRE(node.outputs.front().connectionKind == ConnectionKind::ConfigurationAttachment);
    REQUIRE(node.outputs.front().attachmentType == AttachmentType::Unison);

    const auto configuration = buildUnisonNodeConfiguration(node.parameters);
    REQUIRE(configuration->layout.order == 3);
    REQUIRE(configuration->layout[0].detuneCents == Catch::Approx(-63.952f));
    REQUIRE(configuration->layout[1].pan == Catch::Approx(0.5f));
    REQUIRE(configuration->layout[2].phaseCycles == Catch::Approx(0.2318667f));

    const auto published = NodeDspConfigurationFactory().create(
            AudioModuleRole::Unison,
            node.parameters,
            {});
    REQUIRE(published != nullptr);
    REQUIRE(published->role() == AudioModuleRole::Unison);
}

TEST_CASE("Unison compiles as Voice Context configuration without a runtime step",
        "[cycle-v2][unison][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Unison, "unison", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "unison", "unison", false },
            { "voice", "unison", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "wave", "context", true }).succeeded());

    const auto compiled = GraphCompiler().compile(graph);

    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.configurationAttachments.size() == 1);
    REQUIRE(compiled.plan.voiceContexts.size() == 1);
    REQUIRE(compiled.plan.voiceContexts.front().lanes.order == 1);
    REQUIRE(std::none_of(
            compiled.plan.steps.begin(), compiled.plan.steps.end(), [](const auto& step) {
                return step.nodeId == "unison";
            }));
    REQUIRE(std::none_of(
            compiled.plan.buffers.begin(), compiled.plan.buffers.end(), [](const auto& buffer) {
                return buffer.sourceNodeId == "unison";
            }));
}

TEST_CASE("Unison configuration fans out without sharing runtime state",
        "[cycle-v2][unison][graph][voice-context]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Unison, "unison", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "first", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "second", {}));
    REQUIRE(GraphEditor().connect(
            graph, { "unison", "unison", false }, { "first", "unison", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph, { "unison", "unison", false }, { "second", "unison", true }).succeeded());

    const auto compiled = GraphCompiler().compile(graph);

    REQUIRE(compiled.succeeded());
    REQUIRE(compiled.plan.configurationAttachments.size() == 2);
    REQUIRE(compiled.plan.voiceContexts.size() == 2);
    REQUIRE(compiled.plan.voiceContexts[0].unison == compiled.plan.voiceContexts[1].unison);
}

TEST_CASE("Unison preview paths use pitch duration detune and exact voice phase",
        "[cycle-v2][unison][preview]") {
    Node node = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    setParameter(node, "order", "3");
    setParameter(node, "width", "5");
    setParameter(node, "phase", "0");
    setParameter(node, "jitter", "0");

    const auto atMiddleC = makeUnisonPreviewPaths(node, { 60, 1.0 });
    const auto octaveUp = makeUnisonPreviewPaths(node, { 72, 1.0 });
    const auto twiceAsLong = makeUnisonPreviewPaths(node, { 60, 2.0 });

    REQUIRE(atMiddleC.size() == 3);
    REQUIRE(atMiddleC[0].detuneCents == Catch::Approx(-5.f));
    REQUIRE(atMiddleC[1].detuneCents == Catch::Approx(0.f));
    REQUIRE(atMiddleC[2].detuneCents == Catch::Approx(5.f));
    REQUIRE(atMiddleC[0].segments.front().endPhaseCycles < 0.0);
    REQUIRE(atMiddleC[1].segments.size() == 1);
    REQUIRE(atMiddleC[1].segments.front().startPhaseCycles == 0.0);
    REQUIRE(atMiddleC[1].segments.front().endPhaseCycles == 0.0);
    REQUIRE(atMiddleC[2].segments.front().endPhaseCycles > 0.0);
    REQUIRE(travelledCycles(octaveUp[2])
            == Catch::Approx(travelledCycles(atMiddleC[2]) * 2.0));
    REQUIRE(travelledCycles(twiceAsLong[2])
            == Catch::Approx(travelledCycles(atMiddleC[2]) * 2.0));
}

TEST_CASE("Bypassed Unison preview retains its configured paths",
        "[cycle-v2][unison][preview]") {
    Node node = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    setParameter(node, "enabled", "0");
    setParameter(node, "order", "10");

    const auto paths = makeUnisonPreviewPaths(node);

    REQUIRE(paths.size() == 10);
    REQUIRE(paths.front().detuneCents < 0.f);
    REQUIRE(paths.back().detuneCents > 0.f);
}

TEST_CASE("Unison preview bends from the supplied Voice Context pitch trajectory",
        "[cycle-v2][unison][preview][voice-context]") {
    Node node = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    setParameter(node, "order", "2");
    setParameter(node, "width", "5");
    setParameter(node, "phase", "0");
    setParameter(node, "jitter", "0");

    const UnisonPreviewContext neutral { 60, 1.0, { 0.5f, 0.5f, 0.5f } };
    const UnisonPreviewContext rising { 60, 1.0, { 0.5f, 0.75f, 1.f } };
    const auto neutralPaths = makeUnisonPreviewPaths(node, neutral);
    const auto risingPaths = makeUnisonPreviewPaths(node, rising);

    REQUIRE(travelledCycles(risingPaths.back()) > travelledCycles(neutralPaths.back()));
}
