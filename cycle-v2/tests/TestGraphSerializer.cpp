#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Trimesh/TrimeshMeshState.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>

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
    REQUIRE(parameterValueForNode(*voice, "voices") == "6");
    REQUIRE(mesh->inputs.size() == 5);
    REQUIRE(mesh->inputs[1].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(mesh->model != nullptr);
    REQUIRE(mesh->model->schemaId() == "trimesh");

    const var json = serializer.writeJSON(loaded.graph);
    const auto* nodes = json.getProperty("nodes", {}).getArray();
    REQUIRE(nodes != nullptr);
    const var voiceJson = nodes->getReference(0);
    REQUIRE(voiceJson.getProperty("parameters", {}).getProperty("voices", {}).isInt());
    REQUIRE(voiceJson.getProperty("parameters", {}).getProperty("domain", {}).isString());
    REQUIRE(voiceJson.getProperty("inputs", {}).isVoid());
}

TEST_CASE("A Trimesh vertex edit has a localized canonical JSON diff", "[cycle-v2][graph]") {
    GraphSerializer serializer;
    NodeGraph graph = NodeGraph::createDemoGraph();
    const String before = serializer.toJsonString(graph);
    const auto current = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            graph.findNode("waveMesh")->model);
    REQUIRE(current != nullptr);

    Mesh edited;
    REQUIRE(edited.readJSON(current->meshJSON()));
    edited.getVerts().front()->values[Vertex::Amp] += 0.01f;
    REQUIRE(graph.replaceNodeModel(
            "waveMesh",
            std::make_shared<const TrimeshNodeModelState>(edited.writeJSON(), 2)));
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
    REQUIRE(edited.readJSON(current->meshJSON()));
    edited.getVerts().front()->values[Vertex::Amp] = 1.149999976158142f;
    REQUIRE(graph.replaceNodeModel(
            "waveMesh",
            std::make_shared<const TrimeshNodeModelState>(edited.writeJSON(), 2)));
    edited.destroy();

    const String encoded = serializer.toJsonString(graph);
    REQUIRE(encoded.contains("\"amp\": 1.15"));
    REQUIRE_FALSE(encoded.contains("1.149999976158142"));
    REQUIRE(encoded.contains("{ \"time\":"));
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

TEST_CASE("Every bundled graph is canonical JSON and compiles", "[cycle-v2][graph]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    for (const String& name : {
                String("default.cyclegraph"),
                String("with-spies.cyclegraph"),
                String("fft-sawtooth.cyclegraph") }) {
        const File file = resource(name);
        REQUIRE(file.existsAsFile());
        const String encoded = file.loadFileAsString();
        const GraphLoadResult loaded = GraphSerializer().loadJsonString(encoded);
        INFO(name << ": " << (loaded.issues.empty() ? String() : loaded.issues.front().message));
        REQUIRE(loaded.succeeded());
        REQUIRE(GraphValidator().isValid(loaded.graph));
        REQUIRE(GraphCompiler().compile(loaded.graph).succeeded());
        REQUIRE(GraphSerializer().toJsonString(loaded.graph) == encoded);
        REQUIRE_FALSE(encoded.contains("&quot;"));
        REQUIRE_FALSE(encoded.contains("mesh.topology"));
        REQUIRE_FALSE(encoded.contains("curve.modelSnapshot"));
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
