#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Runtime/GraphAudioExecutor.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex.h>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace CycleV2;

namespace {

GraphLoadResult loadObject(std::unique_ptr<DynamicObject> object) {
    return GraphSerializer().readJSON(var(object.release()));
}

File resource(const String& name) {
  #if defined(CYCLE_V2_SOURCE_DIR)
    return File(String(CYCLE_V2_SOURCE_DIR)).getChildFile("resources").getChildFile(name);
  #else
    return File();
  #endif
}

File contentPreset(const String& name) {
  #if defined(CYCLE_V2_SOURCE_DIR)
    return File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile(name);
  #else
    return File();
  #endif
}

bool hasGuideAssignment(
        const NodeGraph& graph,
        const String& guideId,
        const String& destination,
        const String& port) {
    const String prefix = "guide.cube.";
    if (!port.startsWith(prefix)) {
        return false;
    }
    const String suffix = port.fromFirstOccurrenceOf(prefix, false, false);
    const int cubeIndex = suffix.upToFirstOccurrenceOf(".", false, false).getIntValue();
    const String field = suffix.fromFirstOccurrenceOf(".", false, false);
    const auto expectedField = field == "time" ? GuideCurveField::Time
            : field == "red" ? GuideCurveField::Red
            : field == "blue" ? GuideCurveField::Blue
            : field == "phase" ? GuideCurveField::Phase
            : field == "amp" ? GuideCurveField::Amplitude
            : GuideCurveField::Curve;
    return std::any_of(
            graph.getGuideAssignments().begin(), graph.getGuideAssignments().end(),
            [&](const GuideCurveAssignment& assignment) {
                return assignment.guideId == guideId
                        && assignment.targets(destination, { cubeIndex, expectedField });
            });
}

size_t preparedGuideAssignmentCount(const GraphCompileResult& compiled) {
    size_t result {};
    for (const auto& step : compiled.plan.steps) {
        const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
                step.configuration.value);
        if (configuration != nullptr) {
            result += configuration->guideAssignmentCount;
        }
    }

    return result;
}

}

TEST_CASE("Graph JSON is canonical and byte stable", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const String encoded = serializer.toJsonString(source);
    const GraphLoadResult loaded = serializer.loadJsonString(encoded);

    REQUIRE(loaded.succeeded());
    REQUIRE(encoded.trimStart().startsWithChar('{'));
    REQUIRE(encoded.endsWith("\n"));
    REQUIRE(encoded.contains("\"format\": \"cycle-v2-graph\""));
    REQUIRE_FALSE(encoded.contains("<cycleV2Graph"));
    REQUIRE_FALSE(encoded.contains("&quot;"));
    REQUIRE(serializer.toJsonString(loaded.graph) == encoded);
}

TEST_CASE("Graph documents save canonical JSON with stable line endings",
        "[cycle-v2][graph]") {
    const File destination = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-canonical-graph", ".cyclegraph");
    GraphDocument document(NodeGraph::createDemoGraph());

    REQUIRE(document.save(destination));
    REQUIRE(destination.loadFileAsString() == document.toJson());
    REQUIRE(destination.deleteFile());
}

TEST_CASE("Graph JSON derives immutable node names from definitions", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    REQUIRE(nodes != nullptr);
    REQUIRE_FALSE(nodes->isEmpty());
    nodes->getReference(0).getDynamicObject()->setProperty("title", "Preset Override");

    const GraphLoadResult loaded = serializer.readJSON(encoded);
    REQUIRE(loaded.succeeded());
    REQUIRE(labelForNodeKind(loaded.graph.getNodes().front().kind) == "Voice Context");
    REQUIRE_FALSE(serializer.toJsonString(loaded.graph).contains("\"title\""));
}

TEST_CASE("Graph JSON restores definition-owned structure and typed scalars", "[cycle-v2][graph]") {
    const GraphSerializer serializer;
    const GraphLoadResult loaded = serializer.loadJsonString(
            serializer.toJsonString(NodeGraph::createDemoGraph()));

    REQUIRE(loaded.succeeded());
    const Node* voice = loaded.graph.findNode("voice");
    const Node* mesh = loaded.graph.findNode("waveMesh");
    REQUIRE(voice != nullptr);
    REQUIRE(mesh != nullptr);
    REQUIRE(voice->outputs.size() == 1);
    REQUIRE(voice->outputs.front().domain == PortDomain::DomainContext);
    REQUIRE(mesh->inputs.size() == 5);
    REQUIRE(mesh->inputs[1].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(mesh->model != nullptr);
    REQUIRE(mesh->model->schemaId() == "trimesh");

    const var json = serializer.writeJSON(loaded.graph);
    const auto* nodes = json.getProperty("nodes", {}).getArray();
    REQUIRE(nodes != nullptr);
    const var voiceJson = nodes->getReference(0);
    REQUIRE(voiceJson.getProperty("parameters", {}).getProperty("octave", {}).isInt());
    REQUIRE(voiceJson.getProperty("parameters", {}).getProperty("domain", {}).isString());
    REQUIRE(voiceJson.getProperty("inputs", {}).isVoid());
}

TEST_CASE("Legacy Envelope purpose migration is canonical and deduplicates attachments",
        "[cycle-v2][graph][envelope][purpose][migration]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "legacyControl", {}));
    graph.addEdge({
            "scratchEnv",
            "env",
            "waveMesh",
            "scratch",
            PortDomain::EnvelopeSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::ScratchEnvelope
    });

    GraphSerializer serializer;
    var encoded = serializer.writeJSON(graph);
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    REQUIRE(nodes != nullptr);
    for (auto& node : *nodes) {
        const String id = node.getProperty("id", {}).toString();
        if (id == "env" || id == "scratchEnv" || id == "legacyControl") {
            auto* parameters = node.getProperty("parameters", {}).getDynamicObject();
            parameters->removeProperty("purpose");
            parameters->setProperty("dynamic", true);
        }
    }

    auto* edges = encoded.getProperty("edges", {}).getArray();
    REQUIRE(edges != nullptr);
    const var duplicateEdge = edges->getReference(0).clone();
    edges->add(duplicateEdge);

    const GraphLoadResult loaded = serializer.readJSON(encoded);
    REQUIRE(loaded.succeeded());
    REQUIRE(envelopePurposeFor(*loaded.graph.findNode("env")) == EnvelopePurpose::Volume);
    REQUIRE(envelopePurposeFor(*loaded.graph.findNode("scratchEnv")) == EnvelopePurpose::Scratch);
    REQUIRE(envelopePurposeFor(*loaded.graph.findNode("legacyControl")) == EnvelopePurpose::Control);
    for (const String& id : { "env", "scratchEnv", "legacyControl" }) {
        const Node* envelope = loaded.graph.findNode(id);
        REQUIRE(envelope != nullptr);
        REQUIRE(std::none_of(
                envelope->parameters.begin(),
                envelope->parameters.end(),
                [](const NodeParameter& parameter) {
                    return parameter.id == "dynamic";
                }));
    }

    const auto duplicateCount = std::count_if(
            loaded.graph.getEdges().begin(),
            loaded.graph.getEdges().end(),
            [](const Edge& edge) {
                return edge.sourceNodeId == "scratchEnv"
                        && edge.destNodeId == "waveMesh"
                        && edge.destPortId == "scratch";
            });
    REQUIRE(duplicateCount == 1);
    const String canonical = serializer.toJsonString(loaded.graph);
    REQUIRE(serializer.toJsonString(serializer.fromJsonString(canonical)) == canonical);
}

TEST_CASE("Graph JSON discards legacy Voice Context polyphony",
        "[cycle-v2][graph][voice-context][migration]") {
    const GraphSerializer serializer;
    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    REQUIRE(nodes != nullptr);
    nodes->getReference(0).getProperty("parameters", {})
            .getDynamicObject()->setProperty("voices", 6);

    const GraphLoadResult loaded = serializer.readJSON(encoded);

    REQUIRE(loaded.succeeded());
    const Node* voice = loaded.graph.findNode("voice");
    REQUIRE(voice != nullptr);
    REQUIRE(parameterValueForNode(*voice, "voices").isEmpty());
}

TEST_CASE("Graph JSON migrates pre-typed format two edge metadata",
        "[cycle-v2][graph][migration]") {
    const GraphSerializer serializer;
    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* edges = encoded.getProperty("edges", {}).getArray();
    REQUIRE(edges != nullptr);
    for (var& edge : *edges) {
        auto* object = edge.getDynamicObject();
        REQUIRE(object != nullptr);
        object->removeProperty("attachmentType");
        if (object->getProperty("connectionKind").toString() == "signal") {
            object->removeProperty("connectionKind");
        }
    }

    const GraphLoadResult loaded = serializer.readJSON(encoded);

    REQUIRE(loaded.succeeded());
    REQUIRE(GraphValidator().isValid(loaded.graph));
    REQUIRE(serializer.toJsonString(loaded.graph).contains("\"attachmentType\""));
}

TEST_CASE("Legacy graph formats are rejected after direct Guide resource conversion",
        "[cycle-v2][graph][serialization][voice-context]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationTriple, "triple", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "triple", "modulation", false },
            { "voice", "modulation", true }).succeeded());
    const GraphSerializer serializer;
    var encoded = serializer.writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 1);
    auto* edges = encoded.getProperty("edges", {}).getArray();
    REQUIRE(edges != nullptr);
    REQUIRE(edges->size() == 1);
    auto* edge = edges->getReference(0).getDynamicObject();
    REQUIRE(edge != nullptr);
    edge->removeProperty("connectionKind");
    edge->removeProperty("attachmentType");

    const GraphLoadResult loaded = serializer.readJSON(encoded);

    REQUIRE_FALSE(loaded.succeeded());
    REQUIRE_FALSE(loaded.issues.empty());
}

TEST_CASE("Graph JSON persists authored port side overrides", "[cycle-v2][graph][layout]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    Node* add = graph.findNodeForEditing("addMag");
    REQUIRE(add != nullptr);
    add->inputs[0].side = PortSide::Top;
    add->inputs[1].side = PortSide::Bottom;
    add->outputs[0].side = PortSide::Top;

    const GraphSerializer serializer;
    const String encoded = serializer.toJsonString(graph);
    REQUIRE(encoded.contains("\"portSides\""));
    const GraphLoadResult loaded = serializer.loadJsonString(encoded);
    REQUIRE(loaded.succeeded());
    const Node* restored = loaded.graph.findNode("addMag");
    REQUIRE(restored != nullptr);
    REQUIRE(restored->inputs[0].side == PortSide::Top);
    REQUIRE(restored->inputs[1].side == PortSide::Bottom);
    REQUIRE(restored->outputs[0].side == PortSide::Top);
    REQUIRE(serializer.toJsonString(loaded.graph) == encoded);

    var malformed = serializer.writeJSON(graph);
    auto* nodes = malformed.getProperty("nodes", {}).getArray();
    auto found = std::find_if(nodes->begin(), nodes->end(), [](const var& node) {
        return node.getProperty("id", {}).toString() == "addMag";
    });
    REQUIRE(found != nodes->end());
    found->getProperty("portSides", {}).getDynamicObject()
            ->getProperty("inputs").getDynamicObject()
            ->setProperty("missing", "left");
    REQUIRE_FALSE(serializer.readJSON(malformed).succeeded());
}

TEST_CASE("Graph JSON excludes application utility layout",
        "[cycle-v2][graph][serialization][keyboard]") {
    const String encoded = GraphSerializer().toJsonString(NodeGraph::createDemoGraph());
    REQUIRE_FALSE(encoded.contains("\"presentation\""));
    REQUIRE_FALSE(encoded.contains("\"performanceKeyboardBounds\""));
}

TEST_CASE("A Trimesh vertex edit has a localized canonical JSON diff", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    NodeGraph graph = NodeGraph::createDemoGraph();
    const String before = serializer.toJsonString(graph);
    const auto current = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            graph.findNode("waveMesh")->model);
    REQUIRE(current != nullptr);

    Mesh edited;
    edited.deepCopy(&current->mesh());
    edited.getVerts().front()->values[Vertex::Amp] += 0.01f;
    REQUIRE(graph.replaceNodeModel(
            "waveMesh",
            TrimeshNodeModelState::copyOf(edited, 2)));
    edited.destroy();

    const StringArray beforeLines = StringArray::fromLines(before);
    const StringArray afterLines = StringArray::fromLines(serializer.toJsonString(graph));
    REQUIRE(beforeLines.size() == afterLines.size());
    int changedLines = 0;
    for (int index = 0; index < beforeLines.size(); ++index) {
        changedLines += beforeLines[index] != afterLines[index] ? 1 : 0;
    }
    REQUIRE(changedLines == 2);
}

TEST_CASE("Graph JSON rounds floats and compacts shallow objects", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto current = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            graph.findNode("waveMesh")->model);
    REQUIRE(current != nullptr);

    Mesh edited;
    edited.deepCopy(&current->mesh());
    edited.getVerts().front()->values[Vertex::Amp] = 1.149999976158142f;
    REQUIRE(graph.replaceNodeModel(
            "waveMesh",
            TrimeshNodeModelState::copyOf(edited, 2)));
    edited.destroy();

    const String encoded = serializer.toJsonString(graph);
    REQUIRE(encoded.contains("\"amp\": 1.15"));
    REQUIRE_FALSE(encoded.contains("1.149999976158142"));
    REQUIRE(encoded.contains("{ \"time\":"));
    REQUIRE(encoded.contains("\"vertexIds\": [ 0, 1, 2, 3, 4, 5, 6, 7 ]"));
}

TEST_CASE("Graph JSON rejects unsupported, unknown, and legacy input atomically", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    REQUIRE(serializer.loadJsonString("<cycleV2Graph/>").issues.front().code
            == GraphLoadCode::InvalidJson);

    var valid = serializer.writeJSON(NodeGraph::createDemoGraph());
    valid.getDynamicObject()->setProperty("formatVersion", 99);
    REQUIRE(serializer.readJSON(valid).issues.front().code == GraphLoadCode::UnsupportedVersion);

    valid = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = valid.getProperty("nodes", {}).getArray();
    nodes->getReference(0).getDynamicObject()->setProperty("kind", "not-installed");
    const GraphLoadResult unknown = serializer.readJSON(valid);
    REQUIRE_FALSE(unknown.succeeded());
    REQUIRE(unknown.graph.getNodes().empty());
    REQUIRE(unknown.issues.front().code == GraphLoadCode::UnknownNodeType);
}

TEST_CASE("Graph JSON rejects malformed models without partial adoption", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    var& mesh = nodes->getReference(1);
    mesh.getProperty("model", {}).getDynamicObject()->setProperty("version", 999);

    const GraphLoadResult result = serializer.readJSON(encoded);
    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.graph.getNodes().empty());
    REQUIRE(result.issues.front().code == GraphLoadCode::InvalidModel);
}

TEST_CASE("Graph JSON reports duplicate identities non-finite values and invalid addresses atomically",
        "[cycle-v2][graph]") {
    GraphSerializer serializer;

    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    nodes->getReference(1).getDynamicObject()->setProperty(
            "id", nodes->getReference(0).getProperty("id", {}));
    GraphLoadResult result = serializer.readJSON(encoded);
    REQUIRE(result.issues.front().code == GraphLoadCode::DuplicateIdentity);
    REQUIRE(result.graph.getNodes().empty());

    encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    nodes = encoded.getProperty("nodes", {}).getArray();
    nodes->getReference(0).getProperty("position", {}).getDynamicObject()->setProperty(
            "x", std::numeric_limits<double>::infinity());
    result = serializer.readJSON(encoded);
    REQUIRE(result.issues.front().code == GraphLoadCode::InvalidSchema);
    REQUIRE(result.graph.getNodes().empty());

    encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* edges = encoded.getProperty("edges", {}).getArray();
    edges->getReference(0).getDynamicObject()->setProperty("sourcePortId", "missing");
    result = serializer.readJSON(encoded);
    REQUIRE(result.issues.front().code == GraphLoadCode::InvalidGraph);
    REQUIRE(result.graph.getNodes().empty());
}

TEST_CASE("Graph JSON rejects incomplete model arrays", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    var encoded = serializer.writeJSON(NodeGraph::createDemoGraph());
    auto* nodes = encoded.getProperty("nodes", {}).getArray();
    var& meshNode = nodes->getReference(1);
    meshNode.getProperty("model", {}).getProperty("mesh", {})
            .getDynamicObject()->removeProperty("vertices");

    const GraphLoadResult result = serializer.readJSON(encoded);
    REQUIRE(result.issues.front().code == GraphLoadCode::InvalidModel);
    REQUIRE(result.graph.getNodes().empty());
}

TEST_CASE("Every shipped graph is canonical JSON and compiles", "[cycle-v2][graph]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    Array<File> graphs {
            contentPreset("african-horn.cyclegraph"),
            contentPreset("alto-sax.cyclegraph"),
            contentPreset("baroque-flute.cyclegraph"),
            contentPreset("stengah.cyclegraph"),
            resource("default.cyclegraph"),
            resource("fft-sawtooth.cyclegraph"),
            resource("with-spies.cyclegraph")
    };

    for (const File& file : graphs) {
        const String name = file.getFileName();
        REQUIRE(file.existsAsFile());
        const String encoded = file.loadFileAsString();
        const GraphLoadResult loaded = GraphSerializer().loadJsonString(encoded);
        INFO(name << ": " << (loaded.issues.empty() ? String() : loaded.issues.front().message));
        REQUIRE(loaded.succeeded());
        REQUIRE(GraphValidator().isValid(loaded.graph));
        REQUIRE(GraphCompiler().compile(loaded.graph).succeeded());
        const String canonical = GraphSerializer().toJsonString(loaded.graph);
        REQUIRE(GraphSerializer().loadJsonString(canonical).succeeded());
        if (encoded != canonical) {
            FAIL(name + " is not canonical JSON");
        }
        REQUIRE_FALSE(encoded.contains("\"title\""));
        REQUIRE_FALSE(encoded.contains("&quot;"));
        REQUIRE_FALSE(encoded.contains("mesh.topology"));
        REQUIRE_FALSE(encoded.contains("curve.modelSnapshot"));
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Legacy preset ports omit disabled effects and preserve delay controls",
          "[cycle-v2][graph][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const GraphSerializer serializer;
    const NodeGraph african = serializer.fromJsonString(
            contentPreset("african-horn.cyclegraph").loadFileAsString());
    const NodeGraph baroque = serializer.fromJsonString(
            contentPreset("baroque-flute.cyclegraph").loadFileAsString());
    const NodeGraph stengah = serializer.fromJsonString(
            contentPreset("stengah.cyclegraph").loadFileAsString());

    for (const NodeGraph* graph : { &african, &baroque, &stengah }) {
        for (const Node& node : graph->getNodes()) {
            REQUIRE(parameterValueForNode(node, "enabled") != "0");
        }
        const auto modulationEdges = std::count_if(
                graph->getEdges().begin(),
                graph->getEdges().end(),
                [](const Edge& edge) {
                    return edge.sourceNodeId == "morph";
                });
        REQUIRE(modulationEdges == 1);
        const auto modulationAttachment = std::find_if(
                graph->getEdges().begin(),
                graph->getEdges().end(),
                [](const Edge& edge) {
                    return edge.sourceNodeId == "morph"
                            && edge.sourcePortId == "modulation"
                            && edge.destNodeId == "voice"
                            && edge.destPortId == "modulation";
                });
        REQUIRE(modulationAttachment != graph->getEdges().end());
        REQUIRE(modulationAttachment->connectionKind
                == ConnectionKind::ConfigurationAttachment);
        REQUIRE(modulationAttachment->attachmentType
                == AttachmentType::ModulationTriple);
    }

    REQUIRE(african.findNode("waveshaper") == nullptr);
    REQUIRE(african.findNode("impulseResponse") == nullptr);
    REQUIRE(african.findNode("equalizer") == nullptr);
    REQUIRE(african.findNode("reverb") == nullptr);
    REQUIRE(stengah.findNode("reverb") == nullptr);

    const Node* africanDelay = african.findNode("delay");
    const Node* baroqueDelay = baroque.findNode("delay");
    const Node* stengahDelay = stengah.findNode("delay");
    REQUIRE(africanDelay != nullptr);
    REQUIRE(baroqueDelay != nullptr);
    REQUIRE(stengahDelay != nullptr);
    REQUIRE(parameterValueForNode(*africanDelay, "spin") == "0.692");
    REQUIRE(parameterValueForNode(*baroqueDelay, "time") == "0.5");
    REQUIRE(parameterValueForNode(*baroqueDelay, "feedback") == "0.5");
    REQUIRE(parameterValueForNode(*baroqueDelay, "spinIters") == "0.5");
    REQUIRE(parameterValueForNode(*baroqueDelay, "spin") == "0.5");
    REQUIRE(parameterValueForNode(*baroqueDelay, "wet") == "0.5");
    REQUIRE(parameterValueForNode(*stengahDelay, "time") == "0.584");
    REQUIRE(parameterValueForNode(*stengahDelay, "spinIters") == "0.644");
    REQUIRE(parameterValueForNode(*stengahDelay, "spin") == "0.976");
    REQUIRE(parameterValueForNode(*stengahDelay, "wet") == "0.7");
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("African Horn keeps its populated mesh path in the time domain",
        "[cycle-v2][graph][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const String encoded = contentPreset("african-horn.cyclegraph").loadFileAsString();
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(encoded);
    INFO((loaded.issues.empty() ? String() : loaded.issues.front().message));
    REQUIRE(loaded.succeeded());
    const String migrated = GraphSerializer().toJsonString(loaded.graph);
    REQUIRE(GraphSerializer().loadJsonString(migrated).succeeded());
    REQUIRE(loaded.graph.findNode("fft") == nullptr);
    REQUIRE(loaded.graph.findNode("ifft") == nullptr);
    REQUIRE(loaded.graph.findNode("magnitudeLayer1") == nullptr);
    REQUIRE(loaded.graph.findNode("phaseLayer1") == nullptr);

    const auto directTimePath = std::find_if(
            loaded.graph.getEdges().begin(),
            loaded.graph.getEdges().end(),
            [](const Edge& edge) {
                return edge.sourceNodeId == "timeAdd1"
                        && edge.sourcePortId == "out"
                        && edge.destNodeId == "delay"
                        && edge.destPortId == "time";
            });
    REQUIRE(directTimePath != loaded.graph.getEdges().end());

    const auto guideAssignments = std::count_if(
            loaded.graph.getGuideAssignments().begin(),
            loaded.graph.getGuideAssignments().end(),
            [](const GuideCurveAssignment& assignment) {
                return assignment.guideId == "guide1"
                        && assignment.targetNodeId == "timeLayer2";
            });
    REQUIRE(guideAssignments == 2);
    for (const int cube : { 2, 3 }) {
        const String destination = "guide.cube." + String(cube) + ".phase";
        REQUIRE(hasGuideAssignment(loaded.graph, "guide1", "timeLayer2", destination));
    }

    const GraphCompileResult compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    const GraphAudioResult audio = GraphAudioExecutor().process(
            loaded.graph,
            compiled.plan,
            128);
    REQUIRE(std::any_of(
            audio.output.block.samples.begin(),
            audio.output.block.samples.end(),
            [](float sample) {
                return std::abs(sample) > 0.001f;
            }));

    for (const String& nodeId : { String("timeLayer1"), String("timeLayer2") }) {
        const Node* node = loaded.graph.findNode(nodeId);
        REQUIRE(node != nullptr);
        const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model);
        REQUIRE(model != nullptr);
        REQUIRE(model->mesh().getNumVerts() > 0);
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Baroque Flute preserves every authored guide assignment",
          "[cycle-v2][graph][presets][guides]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const NodeGraph graph = GraphSerializer().fromJsonString(
            contentPreset("baroque-flute.cyclegraph").loadFileAsString());
    for (const int cube : { 1, 3, 4, 5, 6 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide2", "magnitudeLayer1", "guide.cube." + String(cube) + ".amp"));
    }
    for (const int cube : { 1, 4 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide4", "magnitudeLayer2", "guide.cube." + String(cube) + ".amp"));
    }
    for (const int cube : { 0, 2 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide1", "magnitudeLayer3", "guide.cube." + String(cube) + ".time"));
    }
    for (const int cube : { 0, 1 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide1", "phaseLayer1", "guide.cube." + String(cube) + ".time"));
    }

    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(preparedGuideAssignmentCount(compiled) == 11);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Alto Sax preserves every authored guide assignment",
          "[cycle-v2][graph][presets][guides]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const NodeGraph graph = GraphSerializer().fromJsonString(
            contentPreset("alto-sax.cyclegraph").loadFileAsString());
    REQUIRE(hasGuideAssignment(graph, "guide3", "timeLayer1", "guide.cube.2.phase"));
    REQUIRE(hasGuideAssignment(graph, "guide3", "timeLayer1", "guide.cube.3.phase"));
    REQUIRE(hasGuideAssignment(graph, "guide2", "timeLayer1", "guide.cube.3.amp"));
    REQUIRE(hasGuideAssignment(graph, "guide1", "timeLayer1", "guide.cube.4.amp"));
    REQUIRE(hasGuideAssignment(graph, "guide2", "timeLayer1", "guide.cube.5.amp"));
    REQUIRE(hasGuideAssignment(graph, "guide4", "magnitudeLayer1", "guide.cube.0.amp"));
    REQUIRE(hasGuideAssignment(graph, "guide3", "magnitudeLayer1", "guide.cube.2.phase"));
    for (const int cube : { 3, 4, 5, 6, 7, 8, 9 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide4", "magnitudeLayer1", "guide.cube." + String(cube) + ".amp"));
    }
    for (const int cube : { 10, 11, 12 }) {
        REQUIRE(hasGuideAssignment(
                graph, "guide3", "magnitudeLayer1", "guide.cube." + String(cube) + ".phase"));
    }

    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(preparedGuideAssignmentCount(compiled) == 17);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Stengah starts from its populated spectral layers", "[cycle-v2][graph][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            contentPreset("stengah.cyclegraph").loadFileAsString());
    INFO((loaded.issues.empty() ? String() : loaded.issues.front().message));
    REQUIRE(loaded.succeeded());

    const Node* voice = loaded.graph.findNode("voice");
    REQUIRE(voice != nullptr);
    REQUIRE(parameterValueForNode(*voice, "domain") == "spectral");
    REQUIRE(loaded.graph.findNode("timeLayer1") == nullptr);
    REQUIRE(loaded.graph.findNode("fft") == nullptr);
    REQUIRE(loaded.graph.findNode("magnitudeOp1") == nullptr);
    REQUIRE(loaded.graph.findNode("phaseOp1") == nullptr);

    const auto hasEdge = [&loaded](const String& sourceNodeId,
                                   const String& sourcePortId,
                                   const String& destNodeId,
                                   const String& destPortId) {
        return std::any_of(loaded.graph.getEdges().begin(), loaded.graph.getEdges().end(),
                [&](const Edge& edge) {
                    return edge.sourceNodeId == sourceNodeId
                            && edge.sourcePortId == sourcePortId
                            && edge.destNodeId == destNodeId
                            && edge.destPortId == destPortId;
                });
    };
    REQUIRE(hasEdge("voice", "context", "magnitudeLayer1", "context"));
    REQUIRE(hasEdge("voice", "context", "phaseLayer1", "context"));
    REQUIRE(hasEdge("voice", "context", "phaseLayer2", "context"));
    REQUIRE(hasEdge("scratchEnvelope", "env", "magnitudeLayer1", "scratch"));
    REQUIRE(hasEdge("scratchEnvelope", "env", "phaseLayer1", "scratch"));
    REQUIRE(hasEdge("scratchEnvelope", "env", "phaseLayer2", "scratch"));
    REQUIRE(hasGuideAssignment(loaded.graph, "guide1", "phaseLayer1", "guide.cube.0.amp"));
    REQUIRE(hasGuideAssignment(loaded.graph, "guide1", "phaseLayer2", "guide.cube.4.phase"));
    REQUIRE(hasEdge("magnitudeLayer1", "out", "magnitudeLayer1Process", "in"));
    REQUIRE(hasEdge("magnitudeLayer1Process", "out", "ifft", "mag"));
    REQUIRE(hasEdge("phaseLayer1", "out", "phaseLayer1Process", "in"));
    REQUIRE(hasEdge("phaseLayer1Process", "out", "phaseOp2", "left"));
    REQUIRE(hasEdge("phaseLayer2", "out", "phaseLayer2Process", "in"));
    REQUIRE(hasEdge("phaseLayer2Process", "out", "phaseOp2", "right"));

    const Node* phaseLayer1 = loaded.graph.findNode("phaseLayer1");
    const Node* phaseLayer2 = loaded.graph.findNode("phaseLayer2");
    const Node* phaseLayerProcess1 = loaded.graph.findNode("phaseLayer1Process");
    const Node* phaseLayerProcess2 = loaded.graph.findNode("phaseLayer2Process");
    REQUIRE(phaseLayer1 != nullptr);
    REQUIRE(phaseLayer2 != nullptr);
    REQUIRE(phaseLayerProcess1 != nullptr);
    REQUIRE(phaseLayerProcess2 != nullptr);
    REQUIRE(parameterValueForNode(*phaseLayerProcess1, "pan") == "1");
    REQUIRE(parameterValueForNode(*phaseLayerProcess1, "range") == "0.6");
    REQUIRE(parameterValueForNode(*phaseLayerProcess2, "pan") == "0");
    REQUIRE(parameterValueForNode(*phaseLayerProcess2, "range") == "0.575");
    const Node* magnitudeLayer1 = loaded.graph.findNode("magnitudeLayer1");
    const Node* magnitudeLayerProcess = loaded.graph.findNode("magnitudeLayer1Process");
    REQUIRE(magnitudeLayer1 != nullptr);
    REQUIRE(magnitudeLayerProcess != nullptr);
    REQUIRE(parameterValueForNode(*magnitudeLayerProcess, "pan") == "0.5");
    REQUIRE(parameterValueForNode(*magnitudeLayerProcess, "range") == "0.625");
    REQUIRE(parameterValueForNode(*magnitudeLayerProcess, "mode") == "additive");
    const auto phaseModel1 = std::dynamic_pointer_cast<const TrimeshNodeModelState>(phaseLayer1->model);
    const auto phaseModel2 = std::dynamic_pointer_cast<const TrimeshNodeModelState>(phaseLayer2->model);
    REQUIRE(phaseModel1 != nullptr);
    REQUIRE(phaseModel2 != nullptr);
    std::vector<VertCube*> phaseCubes1;
    std::vector<VertCube*> phaseCubes2;
    phaseModel1->mesh().copyElements(phaseCubes1);
    phaseModel2->mesh().copyElements(phaseCubes2);
    REQUIRE(phaseCubes1.size() > 0);
    REQUIRE(phaseCubes2.size() > 4);
    REQUIRE(phaseCubes1[0]->guideCurveChans[Vertex::Amp] == 0);
    REQUIRE(phaseCubes2[4]->guideCurveChans[Vertex::Phase] == 0);
    REQUIRE(loaded.graph.findGuideCurve("guide1") != nullptr);
    REQUIRE(loaded.graph.findGuideCurve("guide2") == nullptr);

    const Node* waveshaper = loaded.graph.findNode("waveshaper");
    REQUIRE(waveshaper != nullptr);
    const auto waveshaperModel = std::dynamic_pointer_cast<const CurveNodeModelState>(waveshaper->model);
    REQUIRE(waveshaperModel != nullptr);
    REQUIRE(waveshaperModel->flatCurve() != nullptr);
    REQUIRE(waveshaperModel->flatCurve()->getVertices().size() == 6);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
