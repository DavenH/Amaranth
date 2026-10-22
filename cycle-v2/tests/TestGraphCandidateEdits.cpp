#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "App/Automation/CycleV2AutomationCommand.h"
#include "App/Automation/NodeAuthoringAutomation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphValidator.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

using namespace CycleV2;

namespace {

NodeGraph candidateGraph() {
    NodeGraph graph;
    GraphNodeFactory factory;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "purpose", "Purpose", "volume").succeeded());
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "logarithmic", "Logarithmic", "1").succeeded());
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext });
    graph.addEdge({ "wave", "out", "shape", "time", PortDomain::TimeSignal });
    graph.addEdge({ "shape", "time", "multiply", "left", PortDomain::TimeSignal });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal });
    graph.addEdge({ "multiply", "out", "voiceOut", "time", PortDomain::TimeSignal });
    graph.addEdge({ "globalIn", "time", "output", "time", PortDomain::TimeSignal });
    for (const auto& issue : GraphValidator().validate(graph)) {
        INFO(issue.message);
        REQUIRE(false);
    }
    REQUIRE(GraphValidator().isValid(graph));
    return graph;
}

NodeAuthoringEdit parameterEdit(const String& nodeId, const String& id, const String& value) {
    return { nodeId, { { id, {}, value } }, {}, {} };
}

var revisedModel(const Node& node, uint64_t revision) {
    var model = node.model->writeJSON();
    model.getDynamicObject()->setProperty("revision", (int64) revision);
    model["state"].getDynamicObject()->setProperty("revision", (int64) revision);
    return model;
}

}

TEST_CASE("Semantic candidate edits publish one undoable stored change",
        "[cycle-v2][candidate-authoring]") {
    GraphDocument document(candidateGraph());
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    const uint64_t revision = document.revision();
    int publications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    var model = revisedModel(*document.graph().findNode("shape"), 2);
    model["state"]["vertices"][1].getDynamicObject()->setProperty("y", 0.3);
    NodeAuthoringEdits request { revision, {
            parameterEdit("voice", "octave", "1"),
            { "shape", { { "pre", {}, "0.65" } }, 1, model }
    } };
    InteractionComplexityDiagnostics::reset();
    const auto result = commands.applyNodeEdits(request);
    REQUIRE(result.succeeded());
    REQUIRE(result.changed);
    REQUIRE(document.revision() == revision + 1);
    REQUIRE(publications == 1);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    const String candidate = document.toJson();
    REQUIRE(candidate != baseline);
    REQUIRE(document.undo());
    REQUIRE(document.toJson() == baseline);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.redo());
    REQUIRE(document.toJson() == candidate);
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(candidate, false));
    REQUIRE(reloaded.toJson() == candidate);
}

TEST_CASE("Failed candidate restores implicit removals history dirty state and publications",
        "[cycle-v2][candidate-authoring][rollback]") {
    GraphDocument document(candidateGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeParameter("voice", "octave", {}, "1").succeeded());
    REQUIRE(document.undo());
    const String before = document.toJson();
    const uint64_t revision = document.revision();
    const uint64_t graphRevision = document.graph().getRevision();
    const bool dirty = document.isDirty();
    int publications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });

    SECTION("A model revision conflict after an implicit routing removal cancels") {
        var conflicting = revisedModel(*document.graph().findNode("shape"), 1);
        conflicting["state"]["vertices"][1].getDynamicObject()->setProperty("y", 0.3);
        const auto result = commands.applyNodeEdits({ revision, {
                parameterEdit("env", "purpose", "scratch"), { "shape", {}, 1, conflicting }
        } });
        REQUIRE(result.code == GraphEditCode::ConflictingRevision);
        REQUIRE(result.editIndex == 1);
        REQUIRE_FALSE(result.changed);
    }
    SECTION("Final graph validation failure cancels preceding valid changes") {
        const auto result = commands.applyNodeEdits({ revision, {
                parameterEdit("env", "purpose", "scratch"),
                parameterEdit("shape", "processingScope", "global")
        } });
        REQUIRE(result.code == GraphEditCode::ValidationRejected);
        REQUIRE_FALSE(result.validationIssues.empty());
        REQUIRE_FALSE(result.changed);
    }
    REQUIRE(document.toJson() == before);
    REQUIRE(document.revision() == revision);
    REQUIRE(document.graph().getRevision() == graphRevision);
    REQUIRE(document.isDirty() == dirty);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.canRedo());
    REQUIRE(publications == 0);
    REQUIRE(document.redo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "octave") == "1");
}

TEST_CASE("A failed typed candidate restores guide assignments pruned by a preceding model edit",
        "[cycle-v2][candidate-authoring][rollback][guides]") {
    NodeGraph graph = candidateGraph();
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshMeshFactory::addVoiceCube(*mesh, 0.2f, 0.8f, 0.2f, 0.8f, 0.5f);
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
    graph.addNode(std::move(node));
    mesh->destroy();
    GuideCurveResource guide;
    guide.id = "guide";
    guide.name = "Guide";
    guide.model = createDefaultGuideCurveModel();
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    for (const int cubeIndex : { 0, 8, 1 }) {
        REQUIRE(graph.assignGuideCurve({ "guide", "mesh", { cubeIndex, GuideCurveField::Amplitude } }));
    }
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String before = document.toJson();
    const uint64_t revision = document.revision();
    const uint64_t graphRevision = document.graph().getRevision();
    int publications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto smaller = TrimeshMeshFactory::createDefaultMesh();
    const var model = TrimeshNodeModelState::copyOf(*smaller, 2)->writeJSON();
    smaller->destroy();
    var conflicting = revisedModel(*document.graph().findNode("shape"), 1);
    conflicting["state"]["vertices"][1].getDynamicObject()->setProperty("y", 0.3);
    const auto result = commands.applyNodeEdits({ revision, {
            { "mesh", {}, 1, model }, { "shape", {}, 1, conflicting }
    } });
    REQUIRE(result.code == GraphEditCode::ConflictingRevision);
    REQUIRE(result.editIndex == 1);
    REQUIRE_FALSE(result.changed);
    REQUIRE(document.toJson() == before);
    REQUIRE(document.revision() == revision);
    REQUIRE(document.graph().getRevision() == graphRevision);
    REQUIRE_FALSE(document.isDirty());
    REQUIRE_FALSE(document.canUndo());
    REQUIRE_FALSE(document.canRedo());
    REQUIRE(publications == 0);
}

TEST_CASE("Candidate preflight rejects invalid typed requests and preserves redo on no-op",
        "[cycle-v2][candidate-authoring]") {
    GraphDocument document(candidateGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeParameter("voice", "octave", {}, "1").succeeded());
    REQUIRE(document.undo());
    const auto revision = document.revision();
    const String before = document.toJson();
    const auto rejected = [&](NodeAuthoringEdits request) {
        const auto result = commands.applyNodeEdits(request);
        REQUIRE_FALSE(result.succeeded());
        REQUIRE_FALSE(result.changed);
        REQUIRE(document.toJson() == before);
        REQUIRE(document.revision() == revision);
        REQUIRE(document.canRedo());
    };
    rejected({ revision - 1, { parameterEdit("voice", "octave", "1") } });
    rejected({ revision, {} });
    rejected({ revision, { parameterEdit("missing", "octave", "1") } });
    rejected({ revision, { parameterEdit("voice", "unknown", "1") } });
    rejected({ revision, { parameterEdit("voice", "octave", "0.5") } });
    rejected({ revision, { parameterEdit("shape", "pre", "junk") } });
    rejected({ revision, { parameterEdit("shape", "pre", "0.5junk") } });
    rejected({ revision, { parameterEdit("shape", "pre", "nan") } });
    rejected({ revision, { parameterEdit("shape", "pre", "0x1p-2") } });
    rejected({ revision, { parameterEdit("voice", "octave", "0.1e1") } });
    rejected({ revision, { parameterEdit("voice", "octave", "1"),
            parameterEdit("voice", "pitch", "0") } });
    rejected({ revision, { { "voice", { { "octave", {}, "1" }, { "octave", {}, "2" } }, {}, {} } } });
    rejected({ revision, { { "shape", {}, 9, document.graph().findNode("shape")->model->writeJSON() } } });
    var malformed = revisedModel(*document.graph().findNode("shape"), 2);
    malformed["state"]["vertices"][0].getDynamicObject()->setProperty("x", "not-a-number");
    rejected({ revision, { parameterEdit("voice", "octave", "1"), { "shape", {}, 1, malformed } } });
    rejected({ revision, std::vector<NodeAuthoringEdit>(maximumNodeAuthoringEdits + 1) });
    NodeAuthoringEdit tooMany { "shape", std::vector<NodeParameter>(maximumNodeAuthoringParameters + 1) };
    rejected({ revision, { tooMany } });
    commands.beginCompoundEdit();
    rejected({ revision, { parameterEdit("voice", "octave", "1") } });
    commands.cancelCompoundEdit();
    commands.beginTransientEdit();
    rejected({ revision, { parameterEdit("voice", "octave", "1") } });
    commands.cancelTransientEdit();
    const auto noop = commands.applyNodeEdits({ revision, { parameterEdit("voice", "octave", "0") } });
    REQUIRE(noop.succeeded());
    REQUIRE_FALSE(noop.changed);
    REQUIRE(document.revision() == revision);
    REQUIRE(document.canRedo());
    REQUIRE(document.toJson() == before);
}

TEST_CASE("Candidate parameter lists use canonical per-node normalization independent of order",
        "[cycle-v2][candidate-authoring][envelope]") {
    String firstCandidate;
    for (const bool reversed : { false, true }) {
        GraphDocument document(candidateGraph());
        GraphCommandDispatcher commands(document);
        const String before = document.toJson();
        std::vector<NodeParameter> parameters {
                { "purpose", {}, "pitch" }, { "logarithmic", {}, "1" }
        };
        if (reversed) {
            std::reverse(parameters.begin(), parameters.end());
        }
        REQUIRE(commands.applyNodeEdits({ document.revision(), {
                { "env", parameters, {}, {} }
        } }).succeeded());
        REQUIRE(parameterValueForNode(*document.graph().findNode("env"), "logarithmic") == "0");
        if (!reversed) {
            firstCandidate = document.toJson();
        } else {
            REQUIRE(document.toJson() == firstCandidate);
        }
        REQUIRE(document.undo());
        REQUIRE(document.toJson() == before);
    }
}

TEST_CASE("Automation candidate representation is strict and uses the command registry",
        "[cycle-v2][candidate-authoring][automation]") {
    NodeAuthoringEdits request;
    String error;
    REQUIRE(decodeNodeAuthoringEdits(JSON::parse(R"({"expectedDocumentRevision":3,"edits":[
        {"nodeId":"voice","parameters":[{"id":"octave","value":"1"}]}]})"), request, error));
    REQUIRE(request.expectedDocumentRevision == 3);
    REQUIRE(request.edits.front().parameters.front().value == "1");
    for (const auto* malformed : {
            R"({"expectedDocumentRevision":"3","edits":[]})",
            R"({"expectedDocumentRevision":3.5,"edits":[]})",
            R"({"expectedDocumentRevision":3,"edits":[{"nodeId":4}]})",
            R"({"expectedDocumentRevision":3,"edits":[{"nodeId":"a","parameters":{}}]})",
            R"({"expectedDocumentRevision":3,"edits":[{"nodeId":"a","parameters":[{"id":"x","value":0.5}]}]})",
            R"({"expectedDocumentRevision":3,"edits":[{"nodeId":"a","model":{},"expectedModelRevision":"1"}]})",
            R"({"expectedDocumentRevision":3,"edits":[{"nodeId":"a","expectedModelRevision":1}]})" }) {
        error.clear();
        REQUIRE_FALSE(decodeNodeAuthoringEdits(JSON::parse(malformed), request, error));
        REQUIRE(error.isNotEmpty());
    }
    const auto command = automationCommandForName("applyNodeEdits");
    REQUIRE(command == CycleV2AutomationCommand::ApplyNodeEdits);
    REQUIRE(automationCommandName(*command) == "applyNodeEdits");
    REQUIRE(automationCommandForName("redo") == CycleV2AutomationCommand::Redo);
}
