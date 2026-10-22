#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include <Curve/Curve.h>
#include <Curve/Mesh/CollisionDetector.h>
#include <Curve/Mesh/MorphDimensions.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/TrimeshAuthoringEdits.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "TrimeshAuthoringTestSupport.h"

using namespace CycleV2;
using TestSupport::authoredTrimeshGraph;

namespace {

TrimeshAuthoringEdits requestFor(const GraphDocument& document,
        std::vector<TrimeshAuthoringEdit> edits) {
    return { "mesh", document.revision(), document.graph().findNode("mesh")->model->revision(),
            std::move(edits) };
}

const Mesh& meshFor(const GraphDocument& document) {
    const auto state = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(state != nullptr);
    return state->mesh();
}

}

TEST_CASE("Headless Trimesh batches match numeric editor geometry in every dimensionality",
        "[cycle-v2][trimesh-authoring]") {
    for (const int dimensions : { 0, 1, 2, 3 }) {
        INFO(dimensions);
        GraphDocument document(authoredTrimeshGraph(dimensions));
        GraphCommandDispatcher commands(document);
        const auto* original = document.graph().findNode("mesh");
        const auto beforeModel = original->model;
        const String baseline = document.toJson();
        const String editor = JSON::toString(original->editorState);
        const auto* guideModel = document.graph().findGuideCurve("guide1")->model.get();
        TrimeshNodeModel editorModel;
        editorModel.syncFromNode(*original);
        CollisionDetector detector(nullptr, CollisionDetector::Time);
        REQUIRE(editorModel.setVertexParameter(0, "vertex.amp", 0.61f, detector));
        REQUIRE(editorModel.setVertexParameter(8, "vertex.curve", 0.35f, detector));
        REQUIRE(editorModel.setVertexGuideGain(16, "guideGain.amp", 0.77f));
        int publications {};
        document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
        const auto result = commands.editTrimesh(requestFor(document, {
                { TrimeshAuthoringEditKind::Vertex, 0, -1, "amp", 0.61f },
                { TrimeshAuthoringEditKind::CubeCurve, -1, 1, {}, 0.35f },
                { TrimeshAuthoringEditKind::GuideGain, 16, -1, "amp", 0.77f }
        }));
        REQUIRE(result.succeeded());
        REQUIRE(result.changed);
        CHECK(publications == 1);
        CHECK(meshFor(document).equals(editorModel.currentMesh()));
        const int mask = MorphDimensions::trimesh(dimensions).cornerMask();
        for (int corner = 0; corner < 8; ++corner) {
            if ((corner & mask) == 0) {
                CHECK(meshFor(document).getCubes().front()->getVertex(corner)->values[Vertex::Amp]
                        == Catch::Approx(0.61f));
            }
        }
        CHECK(document.graph().findNode("mesh")->model->revision() == beforeModel->revision() + 1);
        CHECK(JSON::toString(document.graph().findNode("mesh")->editorState) == editor);
        CHECK(document.graph().findGuideCurve("guide1")->model.get() == guideModel);
        REQUIRE(document.graph().getGuideAssignments().size() == 1);
        CHECK(document.graph().getGuideAssignments()[0].target.cubeIndex == 2);
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
}

TEST_CASE("Headless Trimesh rejection and no-ops preserve geometry selection attachments and redo",
        "[cycle-v2][trimesh-authoring][atomic]") {
    GraphDocument document(authoredTrimeshGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const auto graphRevision = document.graph().getRevision();
    const bool dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    auto request = requestFor(document,
            { { TrimeshAuthoringEditKind::Vertex, 0, -1, "amp", 0.61f } });
    bool noOp {};
    SECTION("Stale document") { --request.expectedDocumentRevision; }
    SECTION("Stale model") { ++request.expectedModelRevision; }
    SECTION("Missing node") { request.nodeId = "missing"; }
    SECTION("Empty request") { request.edits.clear(); }
    SECTION("Oversized request") { request.edits.resize(maximumTrimeshAuthoringEdits + 1); }
    SECTION("Missing vertex") { request.edits[0].vertexIndex = 100000; }
    SECTION("Negative vertex") { request.edits[0].vertexIndex = -1; }
    SECTION("Unknown field") { request.edits[0].field = "unknown"; }
    SECTION("Corner Curve cannot mean cube average") { request.edits[0].field = "curve"; }
    SECTION("Invalid cube after a valid edit") {
        request.edits.push_back({ TrimeshAuthoringEditKind::CubeCurve, -1, 100000, {}, 0.5f });
    }
    SECTION("Nonfinite value after a valid edit") {
        request.edits.push_back({ TrimeshAuthoringEditKind::Vertex, 8, -1, "amp",
                std::numeric_limits<float>::quiet_NaN() });
    }
    SECTION("Out of range") { request.edits[0].value = 1.1f; }
    SECTION("Collision after a valid amplitude edit") {
        request.edits.push_back({ TrimeshAuthoringEditKind::Vertex, 0, -1, "phase", 0.5f });
    }
    SECTION("Exact no-op") {
        request.edits[0].value = meshFor(document).getVerts()[0]->values[Vertex::Amp];
        noOp = true;
    }
    SECTION("A changed field returns to its original value") {
        request.edits.push_back({ TrimeshAuthoringEditKind::Vertex, 0, -1, "amp",
                meshFor(document).getVerts()[0]->values[Vertex::Amp] });
        noOp = true;
    }
    const auto result = commands.editTrimesh(request);
    CHECK(result.succeeded() == noOp);
    CHECK_FALSE(result.changed);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.graph().getRevision() == graphRevision);
    CHECK(document.isDirty() == dirty);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

TEST_CASE("Headless Trimesh edits reject inactive morph fields and active transactions",
        "[cycle-v2][trimesh-authoring][dimensions]") {
    for (const int dimensions : { 0, 1, 2 }) {
        GraphDocument document(authoredTrimeshGraph(dimensions));
        GraphCommandDispatcher commands(document);
        const auto request = requestFor(document,
                { { TrimeshAuthoringEditKind::Vertex, 0, -1, "blue", 0.8f } });
        const String baseline = document.toJson();
        REQUIRE_FALSE(commands.editTrimesh(request).succeeded());
        CHECK(document.toJson() == baseline);
        commands.beginCompoundEdit();
        CHECK_FALSE(commands.editTrimesh(request).succeeded());
        commands.cancelCompoundEdit();
        commands.beginTransientEdit();
        CHECK_FALSE(commands.editTrimesh(request).succeeded());
        commands.cancelTransientEdit();
        CHECK(document.toJson() == baseline);
    }
}

TEST_CASE("Headless Trimesh editing copies only its isolated target once",
        "[cycle-v2][trimesh-authoring][complexity]") {
    for (const int unrelated : { 0, 128 }) {
        GraphDocument document(authoredTrimeshGraph(3, unrelated));
        GraphCommandDispatcher commands(document);
        const auto vertexCount = meshFor(document).getNumVerts();
        const auto cubeCount = meshFor(document).getNumCubes();
        const auto request = requestFor(document, {
                { TrimeshAuthoringEditKind::Vertex, 0, -1, "amp", 0.61f },
                { TrimeshAuthoringEditKind::Vertex, 8, -1, "amp", 0.71f }
        });
        InteractionComplexityDiagnostics::reset();
        REQUIRE(commands.editTrimesh(request).succeeded());
        const auto counts = InteractionComplexityDiagnostics::counts();
        CHECK(counts.graphCopies == 0);
        CHECK(counts.modelSerializations == 0);
        CHECK(counts.meshCopies == 1);
        CHECK(counts.meshVerticesCopied == (uint64_t) vertexCount);
        CHECK(counts.meshCubesCopied == (uint64_t) cubeCount);
    }
}

TEST_CASE("Headless Trimesh value edits change the canonical waveform and undo exactly",
        "[cycle-v2][trimesh-authoring][waveform]") {
    struct CurveTables {
        CurveTables() { Curve::calcTable(); }
        ~CurveTables() { Curve::deleteTable(); }
    } tables;
    GraphDocument document(authoredTrimeshGraph(0));
    GraphCommandDispatcher commands(document);
    const auto render = [&] {
        TrimeshNodeModel model;
        model.syncFromNode(*document.graph().findNode("mesh"));
        Rasterization::RasterizationRequest request;
        request.dimensionCount = 0;
        request.morph = { 0.f, 0.f, 0.f };
        request.primaryViewDimension = Vertex::Time;
        request.cyclic = true;
        Rasterization::TrilinearMeshRasterizer rasterizer;
        auto& result = rasterizer.renderWaveform({ model.currentMesh(), request, 0.f });
        return std::vector<float>(result.waveform.waveY.begin(), result.waveform.waveY.end());
    };
    const auto before = render();
    REQUIRE_FALSE(before.empty());
    REQUIRE(commands.editTrimesh(requestFor(document,
            { { TrimeshAuthoringEditKind::Vertex, 0, -1, "amp", 0.1f } })).succeeded());
    CHECK(render() != before);
    REQUIRE(document.undo());
    CHECK(render() == before);
}
