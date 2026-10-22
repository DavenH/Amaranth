#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

using namespace CycleV2;

namespace {

NodeGraph guidedGraph(int unrelatedNodes = 0) {
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
    graph.addNode(std::move(node));
    mesh->destroy();
    Image image(Image::ARGB, 2, 2, true);
    MemoryOutputStream encoded;
    REQUIRE(PNGImageFormat().writeImageToStream(image, encoded));
    String error;
    const auto heatmap = GuideHeatmapAsset::decode(encoded.getMemoryBlock(), "path.png", error);
    REQUIRE(heatmap != nullptr);
    REQUIRE(graph.addGuideHeatmap(heatmap));
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.shortLabel = "G1";
    guide.name = "Reed closing grain";
    guide.colourIndex = 3;
    guide.shelfOrder = 2;
    guide.enabled = false;
    guide.noise = 0.123456791f;
    guide.dcOffset = 1.e-10f;
    guide.phase = 0.876543224f;
    guide.noiseSeed = 234;
    guide.heatmapAssetId = heatmap->id();
    guide.model = createDefaultGuideCurveModel();
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({ "guide1", "mesh", { 0, GuideCurveField::Amplitude } }));
    for (int index = 0; index < unrelatedNodes; ++index) {
        graph.addNode(GraphNodeFactory().createNode(
                NodeKind::TrilinearMesh, "unrelated" + String(index), {}));
    }
    return graph;
}

GuideCurveEdits requestFor(const GraphDocument& document,
        std::vector<GuideCurvePointEdit> edits) {
    return { "guide1", document.revision(),
            document.graph().findGuideCurve("guide1")->model->revision(), std::move(edits) };
}

const FlatCurveModel& curveFor(const GraphDocument& document) {
    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(
            document.graph().findGuideCurve("guide1")->model);
    REQUIRE(model != nullptr);
    REQUIRE(model->flatCurve() != nullptr);
    return *model->flatCurve();
}

}

TEST_CASE("Targeted Guide points publish one undoable edit without replacing resource state",
        "[cycle-v2][guide-authoring]") {
    GraphDocument document(guidedGraph());
    GraphCommandDispatcher commands(document);
    const auto before = *document.graph().findGuideCurve("guide1");
    const auto mesh = document.graph().findNode("mesh")->model;
    const String baseline = document.toJson();
    const uint64_t revision = document.revision();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    const auto result = commands.editGuideCurve(requestFor(document, {
            { GuideCurvePointEditKind::Set, 1, 0.1f, 0.3f, 0.7f },
            { GuideCurvePointEditKind::Insert, 0, 0.5f, 0.8f, {} },
            { GuideCurvePointEditKind::Remove, 2, {}, {}, {} }
    }));
    REQUIRE(result.succeeded());
    REQUIRE(result.changed);
    CHECK(publications == 1);
    CHECK(document.revision() == revision + 1);
    CHECK(result.changes.nodeIds == std::vector<String> { "mesh" });
    CHECK(result.changes.guidesChanged);
    CHECK(document.graph().findNode("mesh")->model == mesh);
    const auto& points = curveFor(document).getVertices();
    REQUIRE(points.size() == 2);
    CHECK(points[0] == FlatCurveVertex { 1, 0.1f, 0.3f, 0.7f });
    CHECK(points[1] == FlatCurveVertex { 3, 0.5f, 0.8f, 0.f });
    const auto* after = document.graph().findGuideCurve("guide1");
    CHECK(after->model->revision() == before.model->revision() + 1);
    CHECK(after->revision == before.revision + 1);
    CHECK(after->name == before.name);
    CHECK(after->shortLabel == before.shortLabel);
    CHECK(after->colourIndex == before.colourIndex);
    CHECK(after->shelfOrder == before.shelfOrder);
    CHECK(after->enabled == before.enabled);
    CHECK(after->noise == before.noise);
    CHECK(after->dcOffset == before.dcOffset);
    CHECK(after->phase == before.phase);
    CHECK(after->noiseSeed == before.noiseSeed);
    CHECK(after->heatmapAssetId == before.heatmapAssetId);
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
    CHECK(document.graph().getGuideAssignments()[0].guideId == "guide1");
    CHECK(document.graph().getGuideAssignments()[0].target
            == TrimeshCubeComponentGuideTarget { 0, GuideCurveField::Amplitude });
    const String edited = document.toJson();
    REQUIRE(document.undo());
    CHECK(document.toJson() == baseline);
    CHECK_FALSE(document.canUndo());
    REQUIRE(document.redo());
    CHECK(document.toJson() == edited);
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(edited, false));
    CHECK(reloaded.toJson() == edited);
}

TEST_CASE("Targeted Guide rejection and no-ops preserve revisions dirty state and redo",
        "[cycle-v2][guide-authoring][atomic]") {
    GraphDocument document(guidedGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const auto graphRevision = document.graph().getRevision();
    const auto dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto request = requestFor(document, { { GuideCurvePointEditKind::Set, 1, {}, 0.3f, {} } });
    bool succeeds {};
    SECTION("Stale document") { --request.expectedDocumentRevision; }
    SECTION("Stale model") { ++request.expectedModelRevision; }
    SECTION("Missing Guide") { request.guideId = "missing"; }
    SECTION("Empty batch") { request.edits.clear(); }
    SECTION("Bounded batch") { request.edits.resize(maximumGuideCurvePointEdits + 1); }
    SECTION("Missing stable point") { request.edits[0].vertexId = 99; }
    SECTION("Set requires a field") { request.edits[0].y.reset(); }
    SECTION("Nonfinite after a valid edit") {
        request.edits.push_back({ GuideCurvePointEditKind::Set, 2, {},
                std::numeric_limits<float>::quiet_NaN(), {} });
    }
    SECTION("Out of range after a valid edit") {
        request.edits.push_back({ GuideCurvePointEditKind::Set, 2, {}, {}, 1.1f });
    }
    SECTION("Cannot remove the final second point") {
        request.edits.push_back({ GuideCurvePointEditKind::Remove, 2, {}, {}, {} });
    }
    SECTION("Insert requires both coordinates") {
        request.edits.push_back({ GuideCurvePointEditKind::Insert, 0, 0.5f, {}, {} });
    }
    SECTION("Insert cannot set a stable identity") {
        request.edits.push_back({ GuideCurvePointEditKind::Insert, 3, 0.5f, 0.5f, {} });
    }
    SECTION("Remove cannot contain fields") {
        request.edits.push_back({ GuideCurvePointEditKind::Remove, 2, {}, 0.5f, {} });
    }
    SECTION("Exact no-op") { request.edits[0].y = 0.5f; succeeds = true; }
    SECTION("Final geometry no-op") {
        request.edits.push_back({ GuideCurvePointEditKind::Set, 1, {}, 0.5f, {} });
        succeeds = true;
    }
    const auto result = commands.editGuideCurve(request);
    CHECK(result.succeeded() == succeeds);
    CHECK_FALSE(result.changed);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.graph().getRevision() == graphRevision);
    CHECK(document.isDirty() == dirty);
    CHECK_FALSE(document.canUndo());
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

TEST_CASE("Targeted Guide point edits reject active gestures and avoid unrelated graph copies",
        "[cycle-v2][guide-authoring][complexity]") {
    for (const int unrelated : { 0, 128 }) {
        GraphDocument document(guidedGraph(unrelated));
        GraphCommandDispatcher commands(document);
        const auto request = requestFor(document,
                { { GuideCurvePointEditKind::Set, 1, {}, 0.2f, {} } });
        commands.beginCompoundEdit();
        CHECK_FALSE(commands.editGuideCurve(request).succeeded());
        commands.cancelCompoundEdit();
        commands.beginTransientEdit();
        CHECK_FALSE(commands.editGuideCurve(request).succeeded());
        commands.cancelTransientEdit();
        InteractionComplexityDiagnostics::reset();
        REQUIRE(commands.editGuideCurve(request).succeeded());
        const auto counts = InteractionComplexityDiagnostics::counts();
        CHECK(counts.graphCopies == 0);
        CHECK(counts.modelSerializations == 0);
        CHECK(counts.meshVerticesCopied == 2);
        CHECK(counts.meshCubesCopied == 0);
    }
}
