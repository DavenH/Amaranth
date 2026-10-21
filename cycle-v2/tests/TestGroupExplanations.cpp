#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <limits>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"

using namespace CycleV2;

namespace {

NodeGraph groupExplanationGraph(int unrelatedNodes = 0) {
    NodeGraph graph = NodeGraph::createDemoGraph();
    GraphNodeFactory factory;
    for (const String id : { "a", "b", "c", "d", "e" }) {
        graph.addNode(factory.createNode(NodeKind::Add, id, {}));
    }
    REQUIRE(graph.addNodeGroup({ "first", "Timbre", { "a", "b", "c" }, false }));
    REQUIRE(graph.addNodeGroup({ "second", "Output", { "d", "e" }, false }));
    for (int index = 0; index < unrelatedNodes; ++index) {
        graph.addNode(factory.createNode(NodeKind::Envelope, "unrelated" + String(index), {}));
    }
    AudioSampleResource resource { "unrelated-audio", "Unrelated.wav", 48000.0, {} };
    resource.samples.resize(16384);
    REQUIRE(graph.addAudioResource(std::move(resource)));
    return graph;
}

void requireFirstGroupAnnotation(const GraphDocument& document, const String& text,
        std::optional<Point<float>> offset) {
    const NodeGroup* group = document.graph().findNodeGroup("first");
    REQUIRE(group != nullptr);
    REQUIRE(group->rationale == text);
    REQUIRE(group->informationCardOffset == offset);
}

var encodedGroup(GraphDocument& document) {
    return GraphSerializer().writeJSON(document.graph());
}

}

TEST_CASE("Group text and card placement share ordered annotation history and persistence",
        "[cycle-v2][groups][explanations][serialization][undo]") {
    GraphDocument document(groupExplanationGraph());
    GraphCommandDispatcher commands(document);
    const ExplanationTarget target { ExplanationScope::Group, "first" };
    const Point<float> initial(360.f, -24.f);
    const Point<float> moved(430.f, 40.f);
    const auto synthesisRevision = document.graph().getRevision();
    REQUIRE(commands.setExplanationText(target, "Bright attack\nWarm body").changed);
    REQUIRE(commands.setGroupInformationCard("first", initial).changed);
    const File file = File(String(CYCLE_V2_TEST_OUTPUT_DIR)).getChildFile("group-explanation.cyclegraph");
    REQUIRE(document.save(file));
    REQUIRE_FALSE(document.isDirty());
    GraphDocument loaded;
    REQUIRE(loaded.load(file));
    requireFirstGroupAnnotation(loaded, "Bright attack\nWarm body", initial);
    REQUIRE(loaded.toJson() == document.toJson());

    REQUIRE(commands.setExplanationText(target, "Restrained upper harmonics").changed);
    REQUIRE(commands.setGroupInformationCard("first", moved).changed);
    REQUIRE(document.nextUndoIsExplanation());
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Restrained upper harmonics", initial);
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Bright attack\nWarm body", initial);
    REQUIRE_FALSE(document.isDirty());
    REQUIRE(document.redo());
    REQUIRE(commands.setGroupInformationCard("first", std::nullopt).changed);
    requireFirstGroupAnnotation(document, "Restrained upper harmonics", std::nullopt);
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Restrained upper harmonics", initial);
    REQUIRE(commands.setExplanationText(target, {}).changed);
    requireFirstGroupAnnotation(document, {}, initial);
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Restrained upper harmonics", initial);
    REQUIRE(document.graph().getRevision() == synthesisRevision);
    REQUIRE(document.lastChange().explanationsChanged);
    REQUIRE_FALSE(document.lastChange().layoutChanged);
    REQUIRE_FALSE(document.lastChange().topologyChanged);
}

TEST_CASE("Group annotation validation and no-ops preserve the complete document and redo",
        "[cycle-v2][groups][explanations][validation]") {
    GraphDocument document(groupExplanationGraph());
    GraphCommandDispatcher commands(document);
    const ExplanationTarget target { ExplanationScope::Group, "first" };
    REQUIRE(commands.setExplanationText(target, "Draft").changed);
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    REQUIRE_FALSE(commands.setExplanationText(target, {}).changed);
    REQUIRE_FALSE(commands.setGroupInformationCard("first", std::nullopt).changed);
    for (const ExplanationTarget invalid : {
            ExplanationTarget { ExplanationScope::Preset, "first" },
            ExplanationTarget { ExplanationScope::Node, {} },
            ExplanationTarget { ExplanationScope::Group, {} },
            ExplanationTarget { ExplanationScope::Group, "missing" } }) {
        REQUIRE_FALSE(commands.setExplanationText(invalid, "Rejected").succeeded());
    }
    REQUIRE_FALSE(commands.setExplanationText(target,
            String::repeatedString("x", maximumExplanationLength + 1)).succeeded());
    REQUIRE_FALSE(commands.setGroupInformationCard("missing", Point<float>()).succeeded());
    for (const Point<float> invalid : {
            Point<float>(std::numeric_limits<float>::infinity(), 0.f),
            Point<float>(0.f, std::numeric_limits<float>::quiet_NaN()),
            Point<float>(maximumInformationCardOffset + 1.f, 0.f),
            Point<float>(0.f, -maximumInformationCardOffset - 1.f) }) {
        REQUIRE_FALSE(commands.setGroupInformationCard("first", invalid).succeeded());
    }
    commands.beginTransientEdit();
    REQUIRE_FALSE(commands.setExplanationText(target, "During gesture").succeeded());
    REQUIRE_FALSE(commands.setGroupInformationCard("first", Point<float>()).succeeded());
    commands.cancelTransientEdit();
    commands.beginCompoundEdit();
    REQUIRE_FALSE(commands.setExplanationText(target, "During transaction").succeeded());
    REQUIRE_FALSE(commands.setGroupInformationCard("first", Point<float>()).succeeded());
    commands.cancelCompoundEdit();
    REQUIRE(document.toJson() == baseline);
    REQUIRE(document.revision() == revision);
    REQUIRE_FALSE(document.isDirty());
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.canRedo());
    REQUIRE(document.redo());
    REQUIRE(commands.setGroupInformationCard("first",
            Point<float>(maximumInformationCardOffset, -maximumInformationCardOffset)).changed);
}

TEST_CASE("Group annotation loading rejects malformed text and placement atomically",
        "[cycle-v2][groups][explanations][serialization][validation]") {
    const int invalidCase = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
    GraphDocument document(groupExplanationGraph());
    var encoded = encodedGroup(document);
    auto* group = encoded.getProperty("nodeGroups", {})[0].getDynamicObject();
    REQUIRE(group != nullptr);
    if (invalidCase == 0) {
        group->setProperty("rationale", 42);
    } else if (invalidCase == 1) {
        group->setProperty("rationale", String::repeatedString("x", maximumExplanationLength + 1));
    } else if (invalidCase == 2) {
        group->setProperty("informationCardOffset", "100, 200");
    } else {
        auto* offset = new DynamicObject();
        offset->setProperty("x", invalidCase == 3 ? var("20") : var(20));
        if (invalidCase != 4) {
            offset->setProperty("y", invalidCase == 5 ? var(true) : var(30));
        }
        if (invalidCase == 6) {
            offset->setProperty("x", 100000.001);
        } else if (invalidCase == 7) {
            offset->setProperty("y", std::numeric_limits<double>::infinity());
        }
        group->setProperty("informationCardOffset", offset);
    }
    const String baseline = document.toJson();
    const auto revision = document.revision();
    REQUIRE_FALSE(GraphSerializer().readJSON(encoded).succeeded());
    if (invalidCase != 7) {
        REQUIRE_FALSE(document.loadJson(JSON::toString(encoded)));
        REQUIRE(document.toJson() == baseline);
        REQUIRE(document.revision() == revision);
        REQUIRE_FALSE(document.canUndo());
        REQUIRE_FALSE(document.isDirty());
    }
}

TEST_CASE("Old group records omit annotations without changing their behavior",
        "[cycle-v2][groups][explanations][serialization]") {
    GraphDocument document(groupExplanationGraph());
    const var encoded = encodedGroup(document);
    const auto* group = encoded.getProperty("nodeGroups", {})[0].getDynamicObject();
    REQUIRE_FALSE(group->hasProperty("rationale"));
    REQUIRE_FALSE(group->hasProperty("informationCardOffset"));
    const auto loaded = GraphSerializer().readJSON(encoded);
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.findNodeGroup("first")->rationale.isEmpty());
    REQUIRE_FALSE(loaded.graph.findNodeGroup("first")->informationCardOffset.has_value());
    REQUIRE(loaded.graph.groupForNode("d")->id == "second");
}

TEST_CASE("Group pruning and structural undo preserve target-owned explanation state and indexes",
        "[cycle-v2][groups][explanations][undo][index]") {
    GraphDocument document(groupExplanationGraph());
    GraphCommandDispatcher commands(document);
    const Point<float> offset(340.f, 18.f);
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "first" }, "Shared timbre").changed);
    REQUIRE(commands.setGroupInformationCard("first", offset).changed);
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "second" }, "Output role").changed);
    NodeGraph copied(document.graph());
    REQUIRE(copied.findNodeGroup("first")->rationale == "Shared timbre");
    REQUIRE(copied.findNodeGroup("second")->rationale == "Output role");
    NodeGraph assigned;
    assigned = copied;
    REQUIRE(assigned.findNodeGroup("second")->id == "second");
    const NodeGraph overlay = NodeGraph::createEditingOverlay(assigned);
    REQUIRE(overlay.findNodeGroup("first")->informationCardOffset == offset);

    REQUIRE(commands.removeNode("c").succeeded());
    REQUIRE(document.graph().findNodeGroup("first")->nodeIds.size() == 2);
    requireFirstGroupAnnotation(document, "Shared timbre", offset);
    REQUIRE(commands.removeNode("b").succeeded());
    REQUIRE(document.graph().findNodeGroup("first") == nullptr);
    REQUIRE(document.graph().groupForNode("a") == nullptr);
    REQUIRE(document.graph().findNodeGroup("second")->rationale == "Output role");
    REQUIRE(document.graph().groupForNode("d")->id == "second");
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Shared timbre", offset);
    REQUIRE(document.graph().groupForNode("a")->id == "first");
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNodeGroup("first") == nullptr);
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNodeGroup("first")->nodeIds.size() == 3);
    REQUIRE(commands.ungroupNodes("first").succeeded());
    REQUIRE(document.graph().findNodeGroup("first") == nullptr);
    REQUIRE(document.graph().groupForNode("d")->id == "second");
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Shared timbre", offset);
}

TEST_CASE("Removing an inline pan cable restores its pruned group annotation on undo",
        "[cycle-v2][groups][explanations][cable][undo]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "pan", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({ "mesh", "out", "pan", "in", PortDomain::ControlSignal });
    graph.addEdge({ "pan", "out", "ifft", "mag", PortDomain::ControlSignal });
    REQUIRE(graph.addNodeGroup({ "first", "Stereo voice", { "mesh", "pan" }, true }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const Point<float> offset(400.f, 12.f);
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "first" }, "Stereo placement").changed);
    REQUIRE(commands.setGroupInformationCard("first", offset).changed);
    REQUIRE(commands.removeCable({ 0 }).succeeded());
    REQUIRE(document.graph().findNode("pan") == nullptr);
    REQUIRE(document.graph().findNodeGroup("first") == nullptr);
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("pan") != nullptr);
    REQUIRE(document.graph().groupForNode("pan")->id == "first");
    requireFirstGroupAnnotation(document, "Stereo placement", offset);
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNodeGroup("first") == nullptr);
    REQUIRE(document.undo());
    requireFirstGroupAnnotation(document, "Stereo placement", offset);
}

TEST_CASE("Group annotation history never copies synthesis state or rebuilds membership indexes",
        "[cycle-v2][groups][explanations][complexity]") {
    const int unrelatedNodes = GENERATE(0, 128);
    GraphDocument document(groupExplanationGraph(unrelatedNodes));
    GraphCommandDispatcher commands(document);
    const auto synthesisRevision = document.graph().getRevision();
    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "first" }, "Connected mechanism").changed);
    REQUIRE(commands.setGroupInformationCard("first", Point<float>(320.f, 8.f)).changed);
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.redo());
    REQUIRE(document.redo());
    const auto counts = InteractionComplexityDiagnostics::counts();
    REQUIRE(counts.graphCopies == 0);
    REQUIRE(counts.meshCopies == 0);
    REQUIRE(counts.audioSamplesCopied == 0);
    REQUIRE(counts.modelSerializations == 0);
    REQUIRE(counts.nodeLinearScans == 0);
    REQUIRE(counts.validationNodeVisits == 0);
    REQUIRE(counts.validationEdgeVisits == 0);
    REQUIRE(counts.groupIndexRebuilds == 0);
    REQUIRE(document.graph().getRevision() == synthesisRevision);
    REQUIRE(document.graph().groupForNode("a")->id == "first");
}
