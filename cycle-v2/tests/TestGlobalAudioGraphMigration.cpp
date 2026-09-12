#include <catch2/catch_test_macros.hpp>

#include "Graph/GlobalAudioGraphMigration.h"
#include "Graph/GlobalAudioGraphRepresentationMigration.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Graph/NodeParameterMap.h"

using namespace CycleV2;

namespace {

NodeGraph legacyEffectGraph() {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "voice", { 120.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 480.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", { 760.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 1040.f, 80.f }));
    graph.addEdge({
            "voice", "out", "shape", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "shape", "time", "delay", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delay", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    return graph;
}

bool hasEdge(
        const NodeGraph& graph,
        const String& sourceNodeId,
        const String& destinationNodeId) {
    return std::any_of(graph.getEdges().begin(), graph.getEdges().end(), [&](const auto& edge) {
        return edge.sourceNodeId == sourceNodeId
                && edge.destNodeId == destinationNodeId;
    });
}

DynamicObject* encodedNode(var& graph, const String& nodeId) {
    auto* nodes = graph.getProperty("nodes", {}).getArray();
    if (nodes == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(nodes->begin(), nodes->end(), [&](const var& encoded) {
        return encoded.getProperty("id", {}).toString() == nodeId;
    });
    return found != nodes->end() ? found->getDynamicObject() : nullptr;
}

}

TEST_CASE("Global audio migration preserves voice layout and packs one explicit lane",
        "[cycle-v2][graph][global-audio-migration]") {
    NodeGraph graph = legacyEffectGraph();
    const Rectangle<float> voiceBounds = graph.findNode("voice")->bounds;
    graph.findNodeForEditing("shape")->outputs.front().side = PortSide::Bottom;
    graph.findNodeForEditing("delay")->inputs.front().side = PortSide::Top;
    graph.findNodeForEditing("delay")->outputs.front().side = PortSide::Bottom;

    const auto migration = GlobalAudioGraphMigration().migrate(graph);

    REQUIRE(migration.succeeded());
    REQUIRE(migration.migrated);
    REQUIRE(migration.globalNodeIds
            == std::vector<String> { "globalInput", "shape", "delay", "out" });
    REQUIRE(graph.findNode("voice")->bounds == voiceBounds);
    REQUIRE(graph.findNode("globalInput") != nullptr);
    REQUIRE(graph.findNode("voiceOutput") != nullptr);
    REQUIRE(NodeParameterMap(*graph.findNode("shape")).stringValue(
            "processingScope",
            {}) == "global");
    REQUIRE_FALSE(hasEdge(graph, "voice", "shape"));
    REQUIRE(hasEdge(graph, "voice", "voiceOutput"));
    REQUIRE(hasEdge(graph, "globalInput", "shape"));
    REQUIRE(graph.findNode("shape")->outputs.front().side == PortSide::Right);
    REQUIRE(graph.findNode("delay")->inputs.front().side == PortSide::Left);
    REQUIRE(graph.findNode("delay")->outputs.front().side == PortSide::Right);
    REQUIRE(graph.findNode("globalInput")->bounds.getY()
            >= voiceBounds.getBottom() + GlobalAudioGraphMigration::laneGap);

    for (size_t index = 1; index < migration.globalNodeIds.size(); ++index) {
        const auto& previous = *graph.findNode(migration.globalNodeIds[index - 1]);
        const auto& current = *graph.findNode(migration.globalNodeIds[index]);
        REQUIRE(current.bounds.getX() - previous.bounds.getRight()
                >= GlobalAudioGraphMigration::nodeClearance);
    }
    REQUIRE(GraphCompiler().compile(graph).succeeded());

    const String firstSerialization = GraphSerializer().toJsonString(graph);
    const auto repeated = GlobalAudioGraphMigration().migrate(graph);
    REQUIRE(repeated.succeeded());
    REQUIRE_FALSE(repeated.migrated);
    REQUIRE(GraphSerializer().toJsonString(graph) == firstSerialization);
}

TEST_CASE("Format four graphs migrate atomically to the explicit audio graph",
        "[cycle-v2][graph][global-audio-migration][serialization]") {
    GraphSerializer serializer;
    var encoded = serializer.writeJSON(legacyEffectGraph());
    encoded.getDynamicObject()->setProperty("formatVersion", 4);

    const auto loaded = serializer.readJSON(encoded);

    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.findNode("globalInput") != nullptr);
    REQUIRE(loaded.graph.findNode("voiceOutput") != nullptr);
    REQUIRE(hasEdge(loaded.graph, "globalInput", "shape"));
    REQUIRE_FALSE(hasEdge(loaded.graph, "voice", "shape"));
    REQUIRE(hasEdge(loaded.graph, "voice", "voiceOutput"));
    REQUIRE(GraphCompiler().compile(loaded.graph).succeeded());
    REQUIRE((int) serializer.writeJSON(loaded.graph).getProperty("formatVersion", {})
            == GraphSerializer::currentFormatVersion);
}

TEST_CASE("Format five graphs gain one explicit voice terminal",
        "[cycle-v2][graph][global-audio-migration][serialization]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalInput", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 320.f, 240.f }));
    graph.addEdge({
            "globalInput", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    bool expectsVoiceConnection {};
    SECTION("a unique voice terminal is connected") {
        graph.addNode(factory.createNode(NodeKind::WaveSource, "voice", { 80.f, 40.f }));
        expectsVoiceConnection = true;
    }
    SECTION("a silent graph leaves the voice sink disconnected") {
        expectsVoiceConnection = false;
    }

    GraphSerializer serializer;
    var encoded = serializer.writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 5);

    const auto migration = GlobalAudioGraphRepresentationMigration().migrate(encoded);
    const auto loaded = serializer.readJSON(encoded);

    REQUIRE(migration.succeeded());
    REQUIRE(migration.migrated);
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.findNode("voiceOutput") != nullptr);
    REQUIRE(hasEdge(loaded.graph, "voice", "voiceOutput") == expectsVoiceConnection);
}

TEST_CASE("Global migration retains authored routing for a branching graph",
        "[cycle-v2][graph][global-audio-migration][layout]") {
    NodeGraph graph = legacyEffectGraph();
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", {}));
    graph.addEdge({
            "shape", "time", "reverb", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.findNodeForEditing("shape")->outputs.front().side = PortSide::Bottom;
    GraphSerializer serializer;
    var encoded = serializer.writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 4);

    const auto migration = GlobalAudioGraphRepresentationMigration().migrate(encoded);
    const auto* shape = encodedNode(encoded, "shape");

    REQUIRE(migration.succeeded());
    REQUIRE(shape != nullptr);
    REQUIRE(shape->getProperty("portSides")
            .getProperty("outputs", {})
            .getProperty("time", {})
            .toString() == "bottom");
}

TEST_CASE("Ambiguous legacy boundaries are rejected without partial mutation",
        "[cycle-v2][graph][global-audio-migration][failure]") {
    NodeGraph graph = legacyEffectGraph();
    graph.addNode(GraphNodeFactory().createNode(NodeKind::WaveSource, "otherVoice", {}));
    graph.addEdge({
            "otherVoice", "out", "delay", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    const String before = GraphSerializer().toJsonString(graph);

    const auto migration = GlobalAudioGraphMigration().migrate(graph);

    REQUIRE_FALSE(migration.succeeded());
    REQUIRE(migration.error.containsIgnoreCase("multiple"));
    REQUIRE(GraphSerializer().toJsonString(graph) == before);
    REQUIRE(graph.findNode("globalInput") == nullptr);
}
