#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "EnvelopeAuthoringTestSupport.h"
#include "Graph/EnvelopeMeshEditPreparation.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/MeshDimensionality.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"
#include "Nodes/Guide/GuideCurveMeshPreparation.h"

using namespace CycleV2;
using namespace CycleV2::TestSupport;

TEST_CASE("Addressed Envelope edits match the actual numeric panel across dimensions and links",
        "[cycle-v2][envelope-authoring][editor]") {
    ScopedJuceInitialiser_GUI gui;
    EnvelopeAuthoringCurveTables tables;
    for (const int dimensions : { 0, 1, 2, 3 }) {
        for (int mask = 0; mask < 8; ++mask) {
            INFO(dimensions);
            INFO(mask);
            const std::array<bool, 3> links { (mask & 4) != 0, (mask & 2) != 0, (mask & 1) != 0 };
            GraphDocument document(authoredEnvelopeGraph(dimensions));
            GraphCommandDispatcher commands(document);
            const auto* node = document.graph().findNode("env");
            const auto id = envelopeFor(document).getCubeIds()[1];
            CurveEditorWidget widget(NodeKind::Envelope);
            widget.syncFromNode(*node);
            REQUIRE(widget.prepareExpandedPanelComponent(*node, { 0.f, 0.f, 640.f, 400.f }) != nullptr);
            widget.setEnvelopeDimensions(dimensions, links[0]);
            widget.setEnvelopeAxisLinks(links[1], links[2]);
            REQUIRE(widget.setSelectedVertexParameter("vertex.amp", 0.61f));
            REQUIRE(widget.setSelectedVertexParameter("vertex.curve", 0.35f));
            REQUIRE(widget.setSelectedVertexParameter("guideGain.amp", 0.77f));
            const auto ui = std::dynamic_pointer_cast<const CurveNodeModelState>(widget.modelPublication());
            REQUIRE(ui != nullptr);
            REQUIRE(ui->envelope() != nullptr);
            REQUIRE(commands.editEnvelope(envelopeRequest(document, {
                    { EnvelopeAuthoringEditKind::Value, id, 0, "amp", 0.61f },
                    { EnvelopeAuthoringEditKind::Value, id, 0, "curve", 0.35f },
                    { EnvelopeAuthoringEditKind::GuideGain, id, 0, "amp", 0.77f }
            }, links)).succeeded());
            CHECK(envelopeFor(document).getMesh().equals(ui->envelope()->getMesh()));
            const auto* cube = envelopeFor(document).cubeForIdentity(id);
            REQUIRE(cube != nullptr);
            const auto active = MorphDimensions(dimensions);
            for (int corner = 0; corner < 8; ++corner) {
                const bool affected = (!active.active(Vertex::Time) || links[0] || (corner & 4) == 0)
                        && (!active.active(Vertex::Red) || links[1] || (corner & 2) == 0)
                        && (!active.active(Vertex::Blue) || links[2] || (corner & 1) == 0);
                CHECK((cube->getVertex(corner)->values[Vertex::Amp] == 0.61f) == affected);
            }
        }
    }
}

TEST_CASE("Envelope batches preserve authored state identities attachments and exact undo",
        "[cycle-v2][envelope-authoring][atomic]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    const String baseline = document.toJson();
    const auto ids = envelopeFor(document).getCubeIds();
    const auto* guide = document.graph().findGuideCurve("guide1")->model.get();
    const auto* node = document.graph().findNode("env");
    const auto parameters = node->parameters;
    const String editor = JSON::toString(node->editorState);
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    const auto result = commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::Value, ids[5], 0, "phase", 1.75f },
            { EnvelopeAuthoringEditKind::Value, ids[1], 0, "amp", 0.61f },
            { EnvelopeAuthoringEditKind::GuideGain, ids[2], 0, "amp", 0.77f }
    }));
    REQUIRE(result.succeeded());
    CHECK(result.changed);
    CHECK(publications == 1);
    CHECK(envelopeFor(document).getCubeIds() == ids);
    CHECK(envelopeFor(document).selectedCubeId() == ids[1]);
    CHECK(envelopeFor(document).getMesh().getCubes()[5]->getVertex(0)->values[Vertex::Phase]
            == Catch::Approx(1.75f));
    node = document.graph().findNode("env");
    CHECK(node->model->revision() == 2);
    REQUIRE(node->parameters.size() == parameters.size());
    for (size_t index = 0; index < parameters.size(); ++index) {
        CHECK(node->parameters[index].id == parameters[index].id);
        CHECK(node->parameters[index].value == parameters[index].value);
    }
    CHECK(JSON::toString(node->editorState) == editor);
    const auto state = std::dynamic_pointer_cast<const CurveNodeModelState>(node->model);
    CHECK(state->envelopeRed() == Catch::Approx(0.3f));
    CHECK(state->envelopeBlue() == Catch::Approx(0.7f));
    CHECK(document.graph().findGuideCurve("guide1")->model.get() == guide);
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

TEST_CASE("Envelope loop markers synchronize all corners and keep Guide shaped endpoints",
        "[cycle-v2][envelope-authoring][markers]") {
    EnvelopeAuthoringCurveTables tables;
    for (const int dimensions : { 0, 1, 2, 3 }) {
        GraphDocument document(authoredEnvelopeGraph(dimensions));
        GraphCommandDispatcher commands(document);
        const auto ids = envelopeFor(document).getCubeIds();
        const auto baseline = document.toJson();
        REQUIRE(commands.assignGuideCurve("guide1", "env", 1, "time").succeeded());
        REQUIRE(commands.editEnvelope(envelopeRequest(document, {
                { EnvelopeAuthoringEditKind::LoopMarker, ids[1], 0, {}, 0.f, true },
                { EnvelopeAuthoringEditKind::Value, ids[1], 0, "amp", 0.63f }
        }, { false, false, false })).succeeded());
        const auto& model = envelopeFor(document);
        const auto& mesh = model.getMesh();
        const auto* loop = model.cubeForIdentity(ids[1]);
        const auto* sustain = model.cubeForIdentity(ids[3]);
        REQUIRE(EnvelopeEditCore::markerState(mesh, *loop, true));
        REQUIRE(EnvelopeEditCore::markerState(mesh, *sustain, false));
        for (int corner = 0; corner < 8; ++corner) {
            CHECK(sustain->getVertex(corner)->values[Vertex::Amp] == loop->getVertex(corner)->values[Vertex::Amp]);
            CHECK(loop->getVertex(corner)->values[Vertex::Curve] == Catch::Approx(0.4f));
            CHECK(sustain->getVertex(corner)->values[Vertex::Curve] == 1.f);
        }
        REQUIRE(commands.editEnvelope(envelopeRequest(document, {
                { EnvelopeAuthoringEditKind::LoopMarker, ids[4], 0, {}, 0.f, true }
        })).succeeded());
        CHECK(envelopeFor(document).getMesh().loopCubes.empty());
        REQUIRE(document.undo());
        REQUIRE(document.undo());
        REQUIRE(document.undo());
        CHECK(document.toJson() == baseline);
    }
}

TEST_CASE("Envelope rejection and no-ops preserve state and redo",
        "[cycle-v2][envelope-authoring][validation]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto revision = document.revision();
    const auto graphRevision = document.graph().getRevision();
    const bool dirty = document.isDirty();
    int publications {};
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    const auto ids = envelopeFor(document).getCubeIds();
    auto request = envelopeRequest(document,
            { { EnvelopeAuthoringEditKind::Value, ids[1], 0, "amp", 0.61f } });
    bool noOp {};
    SECTION("Stale document") { --request.expectedDocumentRevision; }
    SECTION("Stale model") { ++request.expectedModelRevision; }
    SECTION("Missing node") { request.nodeId = "missing"; }
    SECTION("Empty edits") { request.edits.clear(); }
    SECTION("Oversized edits") { request.edits.resize(maximumEnvelopeAuthoringEdits + 1); }
    SECTION("Dimensions differ") { request.dimensionCount = 2; }
    SECTION("Unknown cube") { request.edits[0].cubeId += 10000; }
    SECTION("Unknown corner") { request.edits[0].corner = 8; }
    SECTION("Unknown field") { request.edits[0].field = "unknown"; }
    SECTION("UI field aliases are not socket fields") { request.edits[0].field = "vertex.amp"; }
    SECTION("Out of range") { request.edits[0].value = 1.1f; }
    SECTION("Nonfinite after a valid edit") {
        request.edits.push_back({ EnvelopeAuthoringEditKind::Value, ids[2], 0, "phase",
                std::numeric_limits<float>::quiet_NaN() });
    }
    SECTION("Exact no-op") {
        request.edits[0].value = envelopeFor(document).cubeForIdentity(ids[1])->getVertex(0)->values[Vertex::Amp];
        request.linkedAxes = { false, false, false };
        noOp = true;
    }
    SECTION("Same marker state") {
        request.edits = { { EnvelopeAuthoringEditKind::SustainMarker, ids[3], 0, {}, 0.f, true } };
        noOp = true;
    }
    const auto result = commands.editEnvelope(request);
    CHECK(result.succeeded() == noOp);
    CHECK_FALSE(result.changed);
    CHECK(document.toJson() == baseline);
    CHECK(document.revision() == revision);
    CHECK(document.graph().getRevision() == graphRevision);
    CHECK(document.isDirty() == dirty);
    CHECK(document.canRedo());
    CHECK(publications == 0);
}

TEST_CASE("Envelope inactive coordinates reject edits and active coordinates replicate hidden corners",
        "[cycle-v2][envelope-authoring][dimensions]") {
    EnvelopeAuthoringCurveTables tables;
    for (const int dimensions : { 0, 1, 2, 3 }) {
        for (const auto* field : { "yellow", "red", "blue" }) {
            GraphDocument document(authoredEnvelopeGraph(dimensions));
            GraphCommandDispatcher commands(document);
            const auto id = envelopeFor(document).getCubeIds()[1];
            const int axis = EnvelopeEditCore::fieldDimension(field);
            const auto result = commands.editEnvelope(envelopeRequest(document,
                    { { EnvelopeAuthoringEditKind::Value, id, 0, field, 0.25f } }));
            const bool active = MorphDimensions(dimensions).active(axis);
            CHECK(result.succeeded() == active);
            if (active) {
                const auto* cube = envelopeFor(document).cubeForIdentity(id);
                for (int corner = 0; corner < 8; ++corner) {
                    if ((corner & MorphDimensions(dimensions).cornerMask()) == 0) {
                        CHECK(cube->getVertex(corner)->values[axis] == 0.25f);
                    }
                }
            }
        }
    }
}

TEST_CASE("Envelope batch staging is independent of unrelated graph size and operation count",
        "[cycle-v2][envelope-authoring][complexity]") {
    EnvelopeAuthoringCurveTables tables;
    for (const int unrelated : { 0, 128 }) {
        GraphDocument document(authoredEnvelopeGraph(3, unrelated));
        GraphCommandDispatcher commands(document);
        const auto& model = envelopeFor(document);
        const auto ids = model.getCubeIds();
        const auto vertexCount = model.getMesh().getNumVerts();
        const auto cubeCount = model.getMesh().getNumCubes();
        InteractionComplexityDiagnostics::reset();
        REQUIRE(commands.editEnvelope(envelopeRequest(document, {
                { EnvelopeAuthoringEditKind::Value, ids[1], 0, "amp", 0.61f },
                { EnvelopeAuthoringEditKind::Value, ids[2], 0, "curve", 0.71f }
        })).succeeded());
        const auto counts = InteractionComplexityDiagnostics::counts();
        CHECK(counts.graphCopies == 0);
        CHECK(counts.modelSerializations == 0);
        CHECK(counts.meshCopies == 1);
        CHECK(counts.meshVerticesCopied == (uint64_t) vertexCount);
        CHECK(counts.meshCubesCopied == (uint64_t) cubeCount);
    }
}

TEST_CASE("Envelope addressed edits change canonical playback geometry and undo restores the waveform",
        "[cycle-v2][envelope-authoring][waveform]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph(2));
    GraphCommandDispatcher commands(document);
    const auto render = [&] {
        EnvelopeNodeModel staged;
        REQUIRE(staged.copyFrom(envelopeFor(document)));
        auto& mesh = staged.getMesh();
        EnvRasterizer rasterizer;
        rasterizer.setMesh(&mesh);
        rasterizer.setDimensionality(2);
        rasterizer.setMorphPosition({ 0.4f, 0.3f, 0.7f });
        rasterizer.updateWaveform(&mesh, 0.f);
        const auto snapshot = rasterizer.snapshotView();
        const auto wave = snapshot.waveY();
        return std::vector<float>(wave.get(), wave.get() + wave.size());
    };
    const auto before = render();
    REQUIRE_FALSE(before.empty());
    const auto id = envelopeFor(document).getCubeIds()[1];
    REQUIRE(commands.editEnvelope(envelopeRequest(document,
            { { EnvelopeAuthoringEditKind::Value, id, 0, "amp", 0.1f } })).succeeded());
    CHECK(render() != before);
    REQUIRE(document.undo());
    CHECK(render() == before);
}

TEST_CASE("Envelope discrete batches reject concurrent compound or transient gestures",
        "[cycle-v2][envelope-authoring][atomic]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    const auto request = envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::Value, envelopeFor(document).getCubeIds()[1], 0, "amp", 0.61f }
    });
    const String baseline = document.toJson();
    commands.beginCompoundEdit();
    CHECK_FALSE(commands.editEnvelope(request).succeeded());
    commands.cancelCompoundEdit();
    commands.beginTransientEdit();
    CHECK_FALSE(commands.editEnvelope(request).succeeded());
    commands.cancelTransientEdit();
    CHECK(document.toJson() == baseline);
}

TEST_CASE("Envelope authoring prepares assigned Guides independent of unrelated resources",
        "[cycle-v2][envelope-authoring][complexity][guides]") {
    EnvelopeAuthoringCurveTables tables;
    for (const int unrelated : { 0, 128 }) {
        auto graph = authoredEnvelopeGraph();
        const auto model = graph.findGuideCurve("guide1")->model;
        for (int index = 0; index < unrelated; ++index) {
            GuideCurveResource guide;
            guide.id = "unattached" + String(index);
            guide.name = "Unused Guide " + String(index);
            guide.shortLabel = "G" + String(index + 2);
            guide.model = model;
            REQUIRE(graph.addGuideCurve(std::move(guide)));
        }
        REQUIRE(graph.assignGuideCurve({ "guide1", "env", { 1, GuideCurveField::Time },
                GuideCurveTargetKind::EnvelopeCubeComponent }));
        GraphDocument document(std::move(graph));
        EnvelopeNodeModel staged;
        REQUIRE(staged.copyFrom(envelopeFor(document)));
        for (auto* vertex : staged.getMesh().getCubes()[1]->lineVerts) {
            vertex->values[Vertex::Curve] = 0.f;
        }
        EnvelopeMeshEditPreparation preparation(staged.getMesh(), document.graph(), "env");
        const auto* provider = dynamic_cast<GuideCurveSnapshotProvider*>(preparation.guideProvider());
        REQUIRE(provider != nullptr);
        CHECK(provider->size() == 1);
        CHECK(staged.getMesh().getCubes()[2]->guideCurveAt(Vertex::Amp) == 0);
        CHECK(staged.getMesh().getCubes()[1]->getVertex(0)->values[Vertex::Curve] == 0.f);
        preparation.restore();
        CHECK(staged.getMesh().getCubes()[2]->guideCurveAt(Vertex::Amp) == -1);
        const auto all = GuideCurveMeshPreparation::apply(staged.getMesh(), document.graph(), "env",
                GuideCurveTargetKind::EnvelopeCubeComponent);
        CHECK(all.provider->size() == unrelated + 1);
    }
}

TEST_CASE("Explicitly disabled Envelope sustain remains absent through further edits and save reload",
        "[cycle-v2][envelope-authoring][markers][codec]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    const auto ids = envelopeFor(document).getCubeIds();
    REQUIRE(commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::SustainMarker, ids[3], 0, {}, 0.f, false }
    })).succeeded());
    CHECK(envelopeFor(document).getMesh().sustainCubes.empty());
    REQUIRE(commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::Value, ids[1], 0, "amp", 0.61f }
    })).succeeded());
    CHECK(envelopeFor(document).getMesh().sustainCubes.empty());
    const String saved = document.toJson();
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(saved, false));
    CHECK(envelopeFor(reloaded).getMesh().sustainCubes.empty());
    CHECK(reloaded.toJson() == saved);
}

TEST_CASE("Graph owned component Guides preserve marked endpoint shape through no-ops edits and reload",
        "[cycle-v2][envelope-authoring][markers][guides][codec]") {
    EnvelopeAuthoringCurveTables tables;
    GraphDocument document(authoredEnvelopeGraph());
    GraphCommandDispatcher commands(document);
    const auto ids = envelopeFor(document).getCubeIds();
    REQUIRE(commands.assignGuideCurve("guide1", "env", 1, "time").succeeded());
    REQUIRE(commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::LoopMarker, ids[1], 0, {}, 0.f, true }
    })).succeeded());
    REQUIRE(commands.setPresetDescription("redo sentinel").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const auto noOp = commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::LoopMarker, ids[1], 0, {}, 0.f, true }
    }));
    REQUIRE(noOp.succeeded());
    CHECK_FALSE(noOp.changed);
    CHECK(document.toJson() == baseline);
    CHECK(document.canRedo());
    REQUIRE(commands.editEnvelope(envelopeRequest(document, {
            { EnvelopeAuthoringEditKind::Value, ids[2], 0, "amp", 0.53f }
    })).succeeded());
    const auto* loop = envelopeFor(document).cubeForIdentity(ids[1]);
    REQUIRE(loop != nullptr);
    CHECK(loop->guideCurveAt(Vertex::Time) == -1);
    for (auto* vertex : loop->lineVerts) {
        CHECK(vertex->values[Vertex::Curve] == Catch::Approx(0.4f));
    }
    const String saved = document.toJson();
    GraphDocument reloaded;
    REQUIRE(reloaded.loadJson(saved, false));
    CHECK(reloaded.toJson() == saved);
    for (auto* vertex : envelopeFor(reloaded).cubeForIdentity(ids[1])->lineVerts) {
        CHECK(vertex->values[Vertex::Curve] == Catch::Approx(0.4f));
    }
}
