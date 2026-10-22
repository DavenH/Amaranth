#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>

#include <Curve/Curve.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "App/EnvelopeAuthoringAutomation.h"
#include "App/MeshTopologyAutomation.h"
#include "App/TrimeshAuthoringAutomation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/MeshTopologyEdits.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"
#include "TrimeshAuthoringTestSupport.h"

using namespace CycleV2;
using TestSupport::authoredTrimeshGraph;

namespace {

struct CurveTables {
    CurveTables() { Curve::calcTable(); }
    ~CurveTables() { Curve::deleteTable(); }
};

const Mesh& meshFor(const GraphDocument& document) {
    return std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model)->mesh();
}

const CurveNodeModelState& envelopeFor(const GraphDocument& document) {
    return *std::dynamic_pointer_cast<const CurveNodeModelState>(
            document.graph().findNode("env")->model);
}

MeshTopologyEdits requestFor(const GraphDocument& document,
        std::vector<MeshTopologyEdit> edits, const String& nodeId = "mesh") {
    return { nodeId, document.revision(), document.graph().findNode(nodeId)->model->revision(),
            std::move(edits) };
}

float gapPhase(const Mesh& mesh, int index = 2) {
    return 0.5f * (mesh.getCubes()[(size_t) index - 1]->getVertex(0)->values[Vertex::Phase]
            + mesh.getCubes()[(size_t) index]->getVertex(0)->values[Vertex::Phase]);
}

NodeGraph envelopeGraph(int selected = 2) {
    NodeGraph graph = authoredTrimeshGraph(0);
    EnvelopeNodeModel model;
    auto& mesh = model.getMesh();
    MorphDimensions::project(mesh, MorphDimensions(2), { 0.f, 0.5f, 0.5f });
    mesh.loopCubes.insert(mesh.getCubes()[1]);
    EnvRasterizer rasterizer(nullptr, "TopologyFixture");
    rasterizer.setMesh(&mesh);
    rasterizer.setMorphPosition({ 0.f, 0.5f, 0.5f });
    rasterizer.setDimensionality(2);
    rasterizer.setCalcDepthDimensions(false);
    rasterizer.updateWaveform(&mesh, 0.f);
    EnvelopeEditCore::validateMarkers(mesh, rasterizer);
    EnvelopeEditCore::synchronizeLoopSeam(mesh, rasterizer, true);
    REQUIRE(model.synchronizeFromMesh(mesh.getCubes()[(size_t) selected]));
    auto node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    auto editor = std::make_unique<DynamicObject>();
    editor->setProperty("selectedCubeId", (int64) *model.selectedCubeId());
    editor->setProperty("sentinel", "preserve");
    node.editorState = var(editor.release());
    node.model = CurveNodeModelState::copyOf(model, 0.5f, 0.5f, 1, node.editorState.clone());
    for (auto& parameter : node.parameters) {
        if (parameter.id == "dimensionCount") {
            parameter.value = "2";
        }
    }
    graph.addNode(std::move(node));
    REQUIRE(graph.assignGuideCurve({ "guide1", "env", { 2, GuideCurveField::Phase },
            GuideCurveTargetKind::EnvelopeCubeComponent }));
    REQUIRE(graph.assignGuideCurve({ "guide1", "env", { 4, GuideCurveField::Amplitude },
            GuideCurveTargetKind::EnvelopeCubeComponent }));
    return graph;
}

}

TEST_CASE("Middle Trimesh topology changes retain Guide targets and exact assignment ordering on undo",
        "[cycle-v2][mesh-topology]") {
    NodeGraph graph = authoredTrimeshGraph(0);
    REQUIRE(graph.assignGuideCurve({ "guide1", "mesh", { 0, GuideCurveField::Phase } }));
    REQUIRE(graph.assignGuideCurve({ "guide1", "mesh", { 5, GuideCurveField::Amplitude } }));
    REQUIRE(graph.assignGuideCurve({ "guide1", "mesh", { 2, GuideCurveField::Curve } }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    const float guidedPhase = meshFor(document).getCubes()[2]->getVertex(0)->values[Vertex::Phase];
    std::vector<MeshTopologyEdit> edits;
    int expectedGuidedIndex {};
    SECTION("Insert before the guided middle cube") {
        edits.push_back({ MeshTopologyEditKind::Insert, 2, 0, gapPhase(meshFor(document)), 0.4f, 0.5f });
        expectedGuidedIndex = 3;
    }
    SECTION("Remove before the guided middle cube") {
        edits.push_back({ MeshTopologyEditKind::Remove, 1 });
        expectedGuidedIndex = 1;
    }
    SECTION("Remove the guided cube") {
        edits.push_back({ MeshTopologyEditKind::Remove, 2 });
        expectedGuidedIndex = -1;
    }
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    const auto result = commands.editMeshTopology(requestFor(document, edits));
    REQUIRE(result.succeeded());
    REQUIRE(result.changed);
    CHECK(publications == 1);
    if (expectedGuidedIndex >= 0) {
        const auto* assignment = document.graph().guideAssignmentForTarget(
                "mesh", { expectedGuidedIndex, GuideCurveField::Amplitude });
        REQUIRE(assignment != nullptr);
        CHECK(assignment->guideId == "guide1");
        CHECK(meshFor(document).getCubes()[(size_t) expectedGuidedIndex]
                ->getVertex(0)->values[Vertex::Phase] == guidedPhase);
        CHECK(document.graph().getGuideAssignments().size() == 4);
    } else {
        CHECK(document.graph().getGuideAssignments().size() == 2);
    }
    const String changed = document.toJson();
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    CHECK_FALSE(document.canUndo());
    REQUIRE(document.redo());
    CHECK(document.toJson() == changed);
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(changed, false));
    CHECK(reloaded.toJson() == changed);
}

TEST_CASE("Trimesh topology preserves shared surviving vertices and remaps explicit selection",
        "[cycle-v2][mesh-topology][selection]") {
    NodeGraph graph = authoredTrimeshGraph(3);
    auto source = std::dynamic_pointer_cast<const TrimeshNodeModelState>(graph.findNode("mesh")->model);
    auto mesh = std::make_unique<Mesh>();
    mesh->deepCopy(&source->mesh());
    auto* shared = mesh->getCubes()[0]->getVertex(4);
    auto* discarded = mesh->getCubes()[1]->getVertex(0);
    REQUIRE(mesh->getCubes()[1]->setVertex(shared, 0));
    REQUIRE(mesh->removeVert(discarded));
    delete discarded;
    shared->values[Vertex::Amp] = 0.333f;
    mesh->validate();
    const int selectedIndex = (int) std::distance(mesh->getVerts().begin(),
            std::find(mesh->getVerts().begin(), mesh->getVerts().end(), shared));
    auto editor = graph.findNode("mesh")->editorState.clone();
    editor.getDynamicObject()->setProperty("selectedVertexId", selectedIndex);
    REQUIRE(graph.replaceNodeEditorState("mesh", editor));
    REQUIRE(graph.replaceNodeModel("mesh", TrimeshNodeModelState::takeOwnership(std::move(mesh), 1)));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    const int vertices = meshFor(document).getNumVerts();
    REQUIRE(commands.editMeshTopology(requestFor(document,
            { { MeshTopologyEditKind::Remove, 0 } })).succeeded());
    const auto& after = meshFor(document);
    const auto* retained = after.getCubes()[0]->getVertex(0);
    CHECK(retained->values[Vertex::Amp] == Catch::Approx(0.333f));
    CHECK(retained->owners.size() == 1);
    CHECK(after.getNumVerts() == vertices - 7);
    const int nextSelected = (int) document.graph().findNode("mesh")->editorState["selectedVertexId"];
    REQUIRE(isPositiveAndBelow(nextSelected, after.getNumVerts()));
    CHECK(after.getVerts()[(size_t) nextSelected] == retained);
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
}

TEST_CASE("Removing a selected Trimesh pole clears its address in the same undoable edit",
        "[cycle-v2][mesh-topology][selection]") {
    GraphDocument document(authoredTrimeshGraph(0));
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    REQUIRE(commands.editMeshTopology(requestFor(document,
            { { MeshTopologyEditKind::Remove, 1 } })).succeeded());
    CHECK((int) document.graph().findNode("mesh")->editorState["selectedVertexId"] == -1);
    CHECK(document.lastChange().editorStateChanged);
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
}

TEST_CASE("Envelope topology preserves stable identities markers and surviving Guide targets",
        "[cycle-v2][mesh-topology][envelope]") {
    CurveTables tables;
    GraphDocument document(envelopeGraph());
    GraphCommandDispatcher commands(document);
    const auto& before = *envelopeFor(document).envelope();
    const auto identities = before.getCubeIds();
    const auto selected = *before.selectedCubeId();
    const auto loopId = identities[1];
    const auto sustainId = identities[3];
    const String baseline = document.toJson();
    const auto result = commands.editMeshTopology(requestFor(document, {
            { MeshTopologyEditKind::Remove, -1, identities[4] },
            { MeshTopologyEditKind::Insert, 1, 0, 0.025f, 0.4f, 0.5f }
    }, "env"));
    REQUIRE(result.succeeded());
    const auto& after = *envelopeFor(document).envelope();
    CHECK(after.getCubeIds().size() == identities.size());
    CHECK(after.cubeForIdentity(identities[4]) == nullptr);
    REQUIRE(after.cubeForIdentity(selected) != nullptr);
    CHECK(after.selectedCubeId() == selected);
    CHECK(after.getCubeIds()[1] > *std::max_element(identities.begin(), identities.end()));
    CHECK(after.getMesh().loopCubes.count(after.cubeForIdentity(loopId)) == 1);
    CHECK(after.getMesh().sustainCubes.count(after.cubeForIdentity(sustainId)) == 1);
    CHECK((uint64_t) (int64) document.graph().findNode("env")->editorState["selectedCubeId"] == selected);
    CHECK(document.graph().guideAssignmentForTarget("env", { 3, GuideCurveField::Phase }) != nullptr);
    CHECK(document.graph().getGuideAssignments().size() == 2);
    const String changed = document.toJson();
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    REQUIRE(document.redo());
    CHECK(document.toJson() == changed);
}

TEST_CASE("Envelope removal clears marker and selection references without reusing the removed identity",
        "[cycle-v2][mesh-topology][envelope][selection]") {
    CurveTables tables;
    GraphDocument document(envelopeGraph(1));
    GraphCommandDispatcher commands(document);
    const auto removed = *envelopeFor(document).envelope()->selectedCubeId();
    const String baseline = document.toJson();
    REQUIRE(commands.editMeshTopology(requestFor(document, {
            { MeshTopologyEditKind::Remove, -1, removed },
            { MeshTopologyEditKind::Insert, 1, 0, 0.05f, 0.6f, 0.5f }
    }, "env")).succeeded());
    const auto& model = *envelopeFor(document).envelope();
    CHECK(model.cubeForIdentity(removed) == nullptr);
    CHECK_FALSE(model.selectedCubeId().has_value());
    CHECK_FALSE(envelopeFor(document).editorJSON().hasProperty("selectedCubeId"));
    CHECK_FALSE(document.graph().findNode("env")->editorState.hasProperty("selectedCubeId"));
    CHECK(model.getMesh().loopCubes.empty());
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
}

TEST_CASE("Trimesh topology creates a pole from factory and edited empty meshes",
        "[cycle-v2][mesh-topology][empty]") {
    NodeGraph graph;
    bool removeInitial {};
    SECTION("Factory-created empty mesh") {
        graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {}));
    }
    SECTION("Remove the entire authored mesh first") {
        graph = authoredTrimeshGraph(0);
        removeInitial = true;
    }
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    if (removeInitial) {
        const int count = meshFor(document).getNumCubes();
        REQUIRE(count > 0);
        REQUIRE(count <= (int) maximumMeshTopologyEdits);
        std::vector<MeshTopologyEdit> removals((size_t) count, { MeshTopologyEditKind::Remove, 0 });
        REQUIRE(commands.editMeshTopology(requestFor(document, removals)).succeeded());
    }
    CHECK(meshFor(document).getNumCubes() == 0);
    CHECK(meshFor(document).getNumVerts() == 0);
    CHECK(document.graph().getGuideAssignments().empty());
    const auto inspected = runTrimeshAuthoringCommand(document, commands,
            JSON::parse(R"({"command":"inspectTrimesh","nodeId":"mesh"})"));
    REQUIRE((bool) inspected["ok"]);
    REQUIRE(inspected["data"]["cubes"].isArray());
    REQUIRE(inspected["data"]["vertices"].isArray());
    CHECK(inspected["data"]["cubes"].size() == 0);
    CHECK(inspected["data"]["vertices"].size() == 0);
    const String empty = document.toJson();
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(empty, false));
    CHECK(reloaded.toJson() == empty);

    REQUIRE(commands.editMeshTopology(requestFor(document,
            { { MeshTopologyEditKind::Insert, 0, 0, 0.25f, 0.7f, 0.6f } })).succeeded());
    const auto& mesh = meshFor(document);
    REQUIRE(mesh.getNumCubes() == 1);
    CHECK(mesh.getNumVerts() == (int) VertCube::numVerts);
    for (const auto* vertex : mesh.getVerts()) {
        REQUIRE(vertex->owners.size() == 1);
        CHECK(vertex->owners[0] == mesh.getCubes().front());
        CHECK(vertex->values[Vertex::Phase] == Catch::Approx(0.25f));
        CHECK(vertex->values[Vertex::Amp] == Catch::Approx(0.7f));
    }
    CHECK(document.graph().getGuideAssignments().empty());
    REQUIRE(document.undo());
    CHECK(document.toJson() == empty);
    if (removeInitial) {
        REQUIRE(document.undo());
        CHECK(document.toJson() == baseline);
    }
}

TEST_CASE("Envelope topology can clear and recreate its first pole without losing identity history",
        "[cycle-v2][mesh-topology][envelope][empty]") {
    CurveTables tables;
    GraphDocument document(envelopeGraph());
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    const auto identities = envelopeFor(document).envelope()->getCubeIds();
    REQUIRE_FALSE(identities.empty());
    REQUIRE(identities.size() <= maximumMeshTopologyEdits);
    const auto largestIdentity = *std::max_element(identities.begin(), identities.end());
    std::vector<MeshTopologyEdit> removals;
    for (const auto identity : identities) {
        removals.push_back({ MeshTopologyEditKind::Remove, -1, identity });
    }
    REQUIRE(commands.editMeshTopology(requestFor(document, removals, "env")).succeeded());
    const auto& cleared = *envelopeFor(document).envelope();
    CHECK(cleared.getMesh().getNumCubes() == 0);
    CHECK(cleared.getMesh().getNumVerts() == 0);
    CHECK(cleared.getCubeIds().empty());
    CHECK_FALSE(cleared.selectedCubeId().has_value());
    CHECK(cleared.getMesh().loopCubes.empty());
    CHECK(cleared.getMesh().sustainCubes.empty());
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
    CHECK(document.graph().getGuideAssignments().front().targetNodeId == "mesh");
    const auto inspected = runEnvelopeAuthoringCommand(document, commands,
            JSON::parse(R"({"command":"inspectEnvelope","nodeId":"env"})"));
    REQUIRE((bool) inspected["ok"]);
    REQUIRE(inspected["data"]["cubes"].isArray());
    CHECK(inspected["data"]["cubes"].size() == 0);
    const String empty = document.toJson();
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(empty, false));
    CHECK(reloaded.toJson() == empty);

    SECTION("Continue editing the live empty model") {
        REQUIRE(commands.editMeshTopology(requestFor(document,
                { { MeshTopologyEditKind::Insert, 0, 0, 0.25f, 0.7f, 0.6f } }, "env")).succeeded());
        const auto& recreated = *envelopeFor(document).envelope();
        REQUIRE(recreated.getCubeIds().size() == 1);
        CHECK(recreated.getCubeIds().front() > largestIdentity);
        CHECK(recreated.getMesh().getNumVerts() == (int) VertCube::numVerts);
        CHECK(recreated.getMesh().loopCubes.empty());
        CHECK(recreated.getMesh().sustainCubes.empty());
        CHECK(document.graph().getGuideAssignments().size() == 1);
        REQUIRE(document.undo());
        CHECK(document.toJson() == empty);
        REQUIRE(document.undo());
        CHECK(document.toJson() == baseline);
    }
    SECTION("Continue editing a reloaded empty model") {
        GraphCommandDispatcher reloadedCommands(reloaded);
        REQUIRE(reloadedCommands.editMeshTopology(requestFor(reloaded,
                { { MeshTopologyEditKind::Insert, 0, 0, 0.25f, 0.7f, 0.6f } }, "env")).succeeded());
        const auto& recreated = *envelopeFor(reloaded).envelope();
        REQUIRE(recreated.getCubeIds().size() == 1);
        CHECK(recreated.getCubeIds().front() > largestIdentity);
        REQUIRE(reloaded.undo());
        CHECK(reloaded.toJson() == empty);
    }
}

TEST_CASE("Rejected topology batches and final no-ops preserve the complete document and redo",
        "[cycle-v2][mesh-topology][atomic]") {
    GraphDocument document(authoredTrimeshGraph(0));
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const auto graphRevision = document.graph().getRevision();
    const auto dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto request = requestFor(document,
            { { MeshTopologyEditKind::Insert, 2, 0, gapPhase(meshFor(document)), 0.4f, 0.5f } });
    bool noOp {};
    SECTION("Stale document") { --request.expectedDocumentRevision; }
    SECTION("Stale model") { ++request.expectedModelRevision; }
    SECTION("No operations") { request.edits.clear(); }
    SECTION("Oversized batch") { request.edits.resize(maximumMeshTopologyEdits + 1); }
    SECTION("Invalid index after an insertion") {
        request.edits.push_back({ MeshTopologyEditKind::Remove, 100000 });
    }
    SECTION("Trimesh indices are not stable IDs") { request.edits[0].cubeId = 1; }
    SECTION("Nonfinite phase") { request.edits[0].phase = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("Out of range amplitude") { request.edits[0].amp = 1.1f; }
    SECTION("Colliding new pole") {
        request.edits[0].phase = meshFor(document).getCubes()[1]->getVertex(0)->values[Vertex::Phase];
    }
    SECTION("Insert and remove the same newly addressed cube is a no-op") {
        request.edits.push_back({ MeshTopologyEditKind::Remove, 2 });
        noOp = true;
    }
    const auto result = commands.editMeshTopology(request);
    CHECK(result.succeeded() == noOp);
    CHECK_FALSE(result.changed);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.graph().getRevision() == graphRevision);
    CHECK(document.isDirty() == dirty);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

TEST_CASE("Topology socket coordinates reject invalid doubles before narrowing",
        "[cycle-v2][mesh-topology][automation]") {
    GraphDocument document(authoredTrimeshGraph(0));
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    var request = JSON::parse(R"({"command":"editMeshTopology","nodeId":"mesh",
            "edits":[{"op":"insert","cubeIndex":2,"phase":0.2,"amp":0.4,"curve":0.5}]})");
    request.getDynamicObject()->setProperty("expectedDocumentRevision", (int64) document.revision());
    request.getDynamicObject()->setProperty("expectedModelRevision", 1);
    SECTION("Amplitude above one") { request["edits"][0].getDynamicObject()->setProperty("amp", 1.0000000001); }
    SECTION("Negative amplitude") { request["edits"][0].getDynamicObject()->setProperty("amp", -1.e-50); }
    SECTION("Phase above canonical range") { request["edits"][0].getDynamicObject()->setProperty("phase", 1.0000000001); }
    SECTION("Fractional insertion index") { request["edits"][0].getDynamicObject()->setProperty("cubeIndex", 2.0); }
    CHECK_FALSE((bool) runMeshTopologyCommand(document, commands, request)["ok"]);
    CHECK(document.toJson() == baseline);
    CHECK_FALSE(document.canUndo());
}

TEST_CASE("Topology socket responses expose final cube addresses including new Envelope identities",
        "[cycle-v2][mesh-topology][automation]") {
    for (const bool envelope : { false, true }) {
        GraphDocument document(envelope ? envelopeGraph() : authoredTrimeshGraph(0));
        GraphCommandDispatcher commands(document);
        const String nodeId = envelope ? "env" : "mesh";
        const int previousCount = envelope ? (int) envelopeFor(document).envelope()->getCubeIds().size()
                : meshFor(document).getNumCubes();
        const float phase = envelope ? 0.02f : gapPhase(meshFor(document));
        auto request = JSON::parse(R"({"command":"editMeshTopology","edits":[
                {"op":"insert","cubeIndex":2,"phase":0.2,"amp":0.4,"curve":0.5}]})");
        request.getDynamicObject()->setProperty("nodeId", nodeId);
        request.getDynamicObject()->setProperty("expectedDocumentRevision", (int64) document.revision());
        request.getDynamicObject()->setProperty("expectedModelRevision", 1);
        request["edits"][0].getDynamicObject()->setProperty("phase", phase);
        const auto response = runMeshTopologyCommand(document, commands, request);
        REQUIRE((bool) response["ok"]);
        const auto addresses = response["data"]["cubeAddresses"];
        REQUIRE(addresses.size() == previousCount + 1);
        CHECK((int) addresses[2]["cubeIndex"] == 2);
        if (envelope) {
            CHECK((int64) addresses[2]["cubeId"] > previousCount);
            CHECK(envelopeFor(document).envelope()->getCubeIds()[2]
                    == (uint64_t) (int64) addresses[2]["cubeId"]);
        } else {
            CHECK_FALSE(addresses[2].hasProperty("cubeId"));
        }
    }
}
