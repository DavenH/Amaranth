#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Graph/NodeModelDecodeDiagnostics.h"
#include "../src/Nodes/Effect2D/CurveNodeEditors.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Effect2D/EnvelopePanelAdapter.h"
#include "../src/Nodes/Effect2D/FlatCurvePanelAdapter.h"
#include "../src/Nodes/Envelope/EnvelopeMeshState.h"
#include "../src/Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../src/Nodes/Effects/EffectSignalProcessors.h"
#include "../src/Nodes/Trimesh/TrimeshNodeModel.h"
#include "../src/Nodes/Waveshaper/WaveshaperSignalProcessor.h"
#include "../src/Runtime/GraphAudioExecutor.h"
#include "../src/Runtime/GraphPreviewExecutor.h"

#include <Curve/Mesh/VertCube.h>
#include <Obj/MorphPosition.h>

using namespace CycleV2;

namespace {

std::vector<CycleV2::NodeParameter> curveControls(const Node& node) {
    return node.parameters;
}

String modelSnapshotForNode(const Node& node) {
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    if (typed == nullptr) {
        return {};
    }
    const var state = typed->flatCurve() != nullptr
            ? typed->flatCurve()->writeJSON()
            : typed->envelope()->writeJSON();
    return JSON::toString(state, false);
}

CurveNodeStatePublication publicationFor(
        const Node& node,
        const String& snapshot,
        uint64_t revision) {
    NodeModelStatePtr model;
    if (node.kind == NodeKind::Envelope) {
        EnvelopeNodeModel domain;
        if (domain.loadSnapshot(snapshot)) {
            model = CurveNodeModelState::copyOf(domain, revision);
        }
    } else {
        FlatCurveModel domain;
        if (domain.loadSnapshot(snapshot)) {
            model = CurveNodeModelState::copyOf(domain, revision);
        }
    }
    return {
        node.id,
        node.model != nullptr ? node.model->revision() : 0,
        std::move(model),
        curveControls(node)
    };
}

}

TEST_CASE("Curve panel adapters make flat and Envelope ownership unambiguous",
          "[cycle-v2][curve-panel-adapters]") {
    FlatCurvePanelAdapter flat(NodeKind::Waveshaper);
    EnvelopePanelAdapter envelope;

    flat.initialiseDefaultMesh();
    envelope.initialiseDefaultMesh();
    REQUIRE(flat.mesh().getNumVerts() == 4);
    REQUIRE(flat.mesh().getNumCubes() == 0);
    REQUIRE(envelope.mesh().getNumCubes() > 0);
}

TEST_CASE("Curve panel adapters synchronize only their typed domain",
          "[cycle-v2][curve-panel-adapters]") {
    GraphNodeFactory factory;
    Node waveshaper = factory.createNode(NodeKind::Waveshaper, "shape", {});
    Node envelopeNode = factory.createNode(NodeKind::Envelope, "env", {});
    FlatCurvePanelAdapter flat(NodeKind::Waveshaper);
    EnvelopePanelAdapter envelope;

    REQUIRE(flat.syncFromNode(waveshaper));
    REQUIRE_FALSE(flat.syncFromNode(envelopeNode));
    REQUIRE(envelope.syncFromNode(envelopeNode));
    REQUIRE_FALSE(envelope.syncFromNode(waveshaper));
    REQUIRE(flat.lastNodeId() == "shape");
    REQUIRE(envelope.lastNodeId() == "env");
}

TEST_CASE("Flat curve models validate atomically and preserve stable selection",
        "[cycle-v2][curve-model]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 10, 0.f, 0.2f, 0.5f },
            { 20, 1.f, 0.8f, 0.5f }
    }));
    REQUIRE(model.selectVertex(20));
    const String before = model.snapshot();
    const uint64_t revision = model.revision();

    REQUIRE_FALSE(model.replaceVertices({
            { 10, 0.f, 0.2f, 0.5f },
            { 10, 1.f, 0.8f, 0.5f }
    }));
    REQUIRE(model.snapshot() == before);
    REQUIRE(model.revision() == revision);

    REQUIRE(model.replaceVertices({
            { 10, 0.f, 0.3f, 0.5f },
            { 20, 1.f, 0.7f, 0.5f }
    }));
    REQUIRE(model.selectedVertexId() == 20);
    REQUIRE(model.getMesh().getNumCubes() == 0);
}

TEST_CASE("Flat curve typed snapshots preserve values but not editor selection",
        "[cycle-v2][curve-model]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.25f, 0.5f }, { 2, 1.f, 0.75f, 0.5f } }));
    REQUIRE(model.selectVertex(2));

    FlatCurveModel restored;
    REQUIRE(restored.loadSnapshot(model.snapshot()));
    REQUIRE(restored.getVertices().size() == 2);
    REQUIRE_FALSE(restored.selectedVertexId().has_value());
    REQUIRE(restored.getVertices()[1].y == 0.75f);
    REQUIRE(restored.selectedMeshVertex() == nullptr);
}

TEST_CASE("Flat curve publication retains identities around inserted vertices",
        "[cycle-v2][curve-model]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.25f, 0.5f }, { 2, 1.f, 0.75f, 0.5f } }));
    const uint64_t firstId = model.getVertices()[0].id;
    const uint64_t lastId = model.getVertices()[1].id;
    auto* inserted = new Vertex(0.5f, 0.5f);
    inserted->values[Vertex::Curve] = 0.5f;
    model.getMesh().addVertex(inserted);
    REQUIRE(model.synchronizeFromMesh(inserted));
    REQUIRE(model.getVertices()[0].id == firstId);
    REQUIRE(model.getVertices()[1].id == lastId);
    REQUIRE(model.getVertices()[2].id != firstId);
    REQUIRE(model.getVertices()[2].id != lastId);
    REQUIRE(model.selectedVertexId() == model.getVertices()[2].id);
}

TEST_CASE("Flat mesh adapter preserves identity through movement curve edits and deletion",
        "[cycle-v2][curve-model][identity]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 10, 0.f, 0.2f, 0.1f },
            { 20, 0.5f, 0.5f, 0.2f },
            { 30, 1.f, 0.8f, 0.3f }
    }));
    Vertex* selected = model.getMesh().getVerts()[1];
    selected->values[Vertex::Phase] = 0.6f;
    selected->values[Vertex::Amp] = 0.7f;
    selected->values[Vertex::Curve] = 0.9f;
    REQUIRE(model.synchronizeFromMesh(selected));
    REQUIRE(model.selectedVertexId() == 20);
    REQUIRE(model.getVertices()[1].id == 20);
    REQUIRE(model.getVertices()[1].x == 0.6f);
    REQUIRE(model.getVertices()[1].curve == 0.9f);

    Vertex* removed = model.getMesh().getVerts().front();
    REQUIRE(model.getMesh().removeVert(removed));
    REQUIRE(model.synchronizeFromMesh(selected));
    delete removed;
    REQUIRE(model.selectedVertexId() == 20);
    REQUIRE(model.getVertices().front().id == 20);

    auto* replacement = new Vertex(0.1f, 0.3f);
    model.getMesh().addVertex(replacement);
    REQUIRE(model.synchronizeFromMesh(selected));
    REQUIRE(model.getMesh().removeVert(selected));
    REQUIRE(model.synchronizeFromMesh(nullptr));
    delete selected;
    REQUIRE_FALSE(model.selectedVertexId().has_value());
}

TEST_CASE("Flat semantic edits are atomic and advance revisions once",
        "[cycle-v2][curve-model][identity]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.2f, 0.5f }, { 2, 1.f, 0.8f, 0.5f } }));
    REQUIRE(model.selectVertex(1));
    Vertex* firstVertex = model.getMesh().getVerts()[0];
    Vertex* secondVertex = model.getMesh().getVerts()[1];
    uint64_t revision = model.revision();
    REQUIRE(model.moveVertex(1, { 0.25f, 0.3f }).succeeded());
    REQUIRE(model.getMesh().getVerts()[0] == firstVertex);
    REQUIRE(model.getMesh().getVerts()[1] == secondVertex);
    REQUIRE(firstVertex->values[Vertex::Phase] == 0.25f);
    REQUIRE(firstVertex->values[Vertex::Amp] == 0.3f);
    REQUIRE(model.revision() == ++revision);
    REQUIRE(model.selectedVertexId() == 1);
    REQUIRE(model.setCurve(1, 0.8f).succeeded());
    REQUIRE(model.getMesh().getVerts()[0] == firstVertex);
    REQUIRE(model.getMesh().getVerts()[1] == secondVertex);
    REQUIRE(firstVertex->values[Vertex::Curve] == 0.8f);
    REQUIRE(model.revision() == ++revision);
    const auto inserted = model.insertVertex({ 0.5f, 0.5f }, 0.4f);
    REQUIRE(inserted.succeeded());
    REQUIRE(model.getMesh().getVerts()[0] == firstVertex);
    REQUIRE(model.getMesh().getVerts()[1] == secondVertex);
    REQUIRE(model.revision() == ++revision);
    REQUIRE(model.removeVertex(inserted.vertexId).succeeded());
    REQUIRE(model.getMesh().getVerts()[0] == firstVertex);
    REQUIRE(model.getMesh().getVerts()[1] == secondVertex);
    REQUIRE(model.revision() == ++revision);

    const String before = model.snapshot();
    REQUIRE_FALSE(model.moveVertex(1, { -1.f, 0.5f }).succeeded());
    REQUIRE_FALSE(model.removeVertex(999).succeeded());
    REQUIRE(model.snapshot() == before);
}

TEST_CASE("Flat point edits preserve document order and never recycle identities",
        "[cycle-v2][curve-model][identity]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 10, 0.1f, 0.2f, 0.5f },
            { 20, 0.5f, 0.4f, 0.5f },
            { 30, 0.9f, 0.6f, 0.5f }
    }));
    Vertex* moved = model.getMesh().getVerts()[0];
    REQUIRE(model.selectVertex(10));

    REQUIRE(model.moveVertex(10, { 0.8f, 0.3f }).succeeded());
    REQUIRE(model.getVertices()[0].id == 10);
    REQUIRE(model.getVertices()[1].id == 20);
    REQUIRE(model.getVertices()[2].id == 30);
    REQUIRE(model.getMesh().getVerts()[0] == moved);
    REQUIRE(model.selectedVertexId() == 10);

    REQUIRE(model.removeVertex(30).succeeded());
    const auto inserted = model.insertVertex({ 0.95f, 0.7f }, 0.5f);
    REQUIRE(inserted.succeeded());
    REQUIRE(inserted.vertexId > 30);
}

TEST_CASE("Invalid live flat edits restore the committed mesh atomically",
        "[cycle-v2][curve-model][identity]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.2f, 0.5f }, { 2, 1.f, 0.8f, 0.5f } }));
    const String before = model.snapshot();
    Vertex* removed = model.getMesh().getVerts().back();
    REQUIRE(model.getMesh().removeVert(removed));
    REQUIRE_FALSE(model.synchronizeFromMesh(nullptr));
    delete removed;

    REQUIRE(model.snapshot() == before);
    REQUIRE(model.getMesh().getNumVerts() == 2);
}

TEST_CASE("Envelope model round trips envelope-only topology without editor intent",
        "[cycle-v2][curve-model][envelope]") {
    EnvelopeNodeModel model;
    model.logarithmic = true;
    model.red = 0.25f;
    model.blue = 0.75f;
    model.redLinked = false;
    model.blueLinked = true;
    const EnvelopeCubeId selectedId = model.getCubeIds()[2];
    REQUIRE(model.selectCube(selectedId));
    const int cubeCount = model.getMesh().getNumCubes();
    const size_t sustainCount = model.getMesh().sustainCubes.size();

    EnvelopeNodeModel restored;
    REQUIRE(restored.loadSnapshot(model.snapshot()));
    REQUIRE(restored.getMesh().getNumCubes() == cubeCount);
    REQUIRE(restored.getMesh().sustainCubes.size() == sustainCount);
    REQUIRE(restored.logarithmic);
    REQUIRE(restored.red == 0.25f);
    REQUIRE(restored.blue == 0.75f);
    REQUIRE_FALSE(restored.redLinked);
    REQUIRE(restored.blueLinked);
    REQUIRE_FALSE(restored.selectedCubeId().has_value());
    REQUIRE(restored.selectedMeshCube() == nullptr);
}

TEST_CASE("Envelope model reads the node-owned dynamic playback policy",
        "[cycle-v2][curve-model][envelope]") {
    GraphNodeFactory factory;
    Node envelope = factory.createNode(NodeKind::Envelope, "env", {});
    for (auto& parameter : envelope.parameters) {
        if (parameter.id == "dynamic") {
            parameter.value = "1";
        }
    }

    EnvelopeNodeModel model;
    REQUIRE(model.syncFromNode(envelope));
    REQUIRE(model.dynamicWhileLive);
}

TEST_CASE("Envelope mesh adapter preserves cube identities across insertion deletion and reorder",
        "[cycle-v2][curve-model][envelope][identity]") {
    EnvelopeNodeModel model;
    auto& cubes = model.getMesh().getCubes();
    VertCube* selectedCube = cubes[2];
    const EnvelopeCubeId selectedId = model.getCubeIds()[2];

    std::swap(cubes[1], cubes[2]);
    REQUIRE(model.synchronizeFromMesh(selectedCube));
    REQUIRE(model.selectedCubeId() == selectedId);
    REQUIRE(model.getCubeIds()[1] == selectedId);

    MorphPosition position;
    position.timeDepth = 0.001f;
    position.redDepth = 1.f;
    position.blueDepth = 1.f;
    auto* inserted = new VertCube(&model.getMesh());
    inserted->initVerts(position);
    model.getMesh().addCube(inserted);
    REQUIRE(model.synchronizeFromMesh(selectedCube));
    const EnvelopeCubeId insertedId = model.getCubeIds().back();
    REQUIRE(insertedId != selectedId);

    VertCube* removed = cubes.front();
    const EnvelopeCubeId removedId = model.getCubeIds().front();
    REQUIRE(model.getMesh().removeCube(removed));
    REQUIRE(model.synchronizeFromMesh(selectedCube));
    delete removed;
    REQUIRE(model.selectedCubeId() == selectedId);
    REQUIRE(std::find(model.getCubeIds().begin(), model.getCubeIds().end(), removedId)
            == model.getCubeIds().end());

    EnvelopeNodeModel restored;
    REQUIRE(restored.loadSnapshot(model.snapshot()));
    REQUIRE(restored.getCubeIds() == model.getCubeIds());
    REQUIRE_FALSE(restored.selectedCubeId().has_value());
}

TEST_CASE("Envelope typed loading rejects malformed state without mutation",
        "[cycle-v2][curve-model]") {
    EnvelopeNodeModel model;
    const String before = model.snapshot();
    REQUIRE_FALSE(model.loadSnapshot("not-json"));
    REQUIRE(model.snapshot() == before);

    var malformed = JSON::parse(before);
    auto* ids = malformed.getDynamicObject()->getProperty("cubeIds").getArray();
    REQUIRE(ids != nullptr);
    ids->set(1, ids->getReference(0));
    REQUIRE_FALSE(model.loadSnapshot(JSON::toString(malformed, false)));
    REQUIRE(model.snapshot() == before);
}

TEST_CASE("Node-specific curve models bind named controls without positional semantics",
        "[cycle-v2][curve-model][controls]") {
    GraphNodeFactory factory;
    Node waveshaperNode = factory.createNode(NodeKind::Waveshaper, "shape", {});
    for (auto& parameter : waveshaperNode.parameters) {
        if (parameter.id == "pre") {
            parameter.value = "0.2";
        }
        if (parameter.id == "post") {
            parameter.value = "0.8";
        }
        if (parameter.id == "aaFactor") {
            parameter.value = "4";
        }
    }
    WaveshaperNodeModel waveshaper;
    waveshaper.syncFromNode(waveshaperNode);
    REQUIRE(waveshaper.preGain == 0.2f);
    REQUIRE(waveshaper.postGain == 0.8f);
    REQUIRE(waveshaper.oversampling == 4);

    Node irNode = factory.createNode(NodeKind::ImpulseResponse, "ir", {});
    for (auto& parameter : irNode.parameters) {
        if (parameter.id == "size") {
            parameter.value = "0.25";
        }
        if (parameter.id == "post") {
            parameter.value = "0.75";
        }
        if (parameter.id == "highPass") {
            parameter.value = "0.1";
        }
    }
    ImpulseResponseNodeModel ir;
    ir.syncFromNode(irNode);
    REQUIRE(ir.size == 0.25f);
    REQUIRE(ir.postGain == 0.75f);
    REQUIRE(ir.highPass == 0.1f);
}

TEST_CASE("Curve model publication is one undoable semantic command",
        "[cycle-v2][curve-model][commands]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.f, 1.f }, { 2, 1.f, 1.f, 1.f } }));
    const auto initialModel = document.graph().findNode("shape")->model;

    REQUIRE(commands.publishCurveState(publicationFor(
            *document.graph().findNode("shape"), model.snapshot(), model.revision())).succeeded());
    REQUIRE(document.canUndo());
    REQUIRE(document.graph().findNode("shape")->model->revision() == model.revision());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("shape")->model->equals(*initialModel));
}

TEST_CASE("Curve drag publications coalesce into one document undo entry",
        "[cycle-v2][curve-model][commands]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.f, 1.f }, { 2, 1.f, 1.f, 1.f } }));

    commands.beginCompoundEdit();
    REQUIRE(commands.publishCurveState(publicationFor(
            *document.graph().findNode("shape"), model.snapshot(), model.revision())).succeeded());
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.1f, 1.f }, { 2, 1.f, 0.9f, 1.f } }));
    REQUIRE(commands.publishCurveState(publicationFor(
            *document.graph().findNode("shape"), model.snapshot(), model.revision())).succeeded());
    commands.commitCompoundEdit();

    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Curve state publication atomically commits controls model and revision",
        "[cycle-v2][curve-state][commands]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.2f, 1.f }, { 2, 1.f, 0.8f, 1.f } }));

    auto publication = publicationFor(*document.graph().findNode("shape"), model.snapshot(), model.revision());
    for (auto& control : publication.controls) {
        if (control.id == "pre") {
            control.value = "0.75";
        }
    }
    int notifications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) {
        ++notifications;
        const Node* observed = document.graph().findNode("shape");
        REQUIRE(parameterValueForNode(*observed, "pre") == "0.75");
        const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(observed->model);
        REQUIRE(typed != nullptr);
        REQUIRE(typed->revision() == model.revision());
    });

    const uint64_t documentRevision = document.revision();
    REQUIRE(commands.publishCurveState(publication).succeeded());
    REQUIRE(document.revision() == documentRevision + 1);
    REQUIRE(notifications == 1);
    document.setListener({});
    REQUIRE(document.undo());
    const Node* restored = document.graph().findNode("shape");
    REQUIRE(parameterValueForNode(*restored, "pre") == "0.5");
    REQUIRE(restored->model->revision() == 1);
}

TEST_CASE("Curve state revisions distinguish retries conflicts and stale writes",
        "[cycle-v2][curve-state][commands]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({ { 1, 0.f, 0.2f, 1.f }, { 2, 1.f, 0.8f, 1.f } }));
    auto publication = publicationFor(*document.graph().findNode("shape"), model.snapshot(), model.revision());
    REQUIRE(commands.publishCurveState(publication).succeeded());

    publication.expectedRevision = model.revision();
    const uint64_t documentRevision = document.revision();
    const auto retry = commands.publishCurveState(publication);
    REQUIRE(retry.succeeded());
    REQUIRE_FALSE(retry.changed);
    REQUIRE(document.revision() == documentRevision);
    REQUIRE_FALSE(document.canRedo());

    auto conflict = publication;
    for (auto& control : conflict.controls) {
        if (control.id == "pre") {
            control.value = "0.75";
        }
    }
    REQUIRE(commands.publishCurveState(conflict).code == GraphEditCode::ConflictingRevision);
    auto stale = publication;
    stale.expectedRevision = 1;
    REQUIRE(commands.publishCurveState(stale).code == GraphEditCode::StaleRevision);
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Curve state publication rejects malformed typed and incomplete control state",
        "[cycle-v2][curve-state][commands]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const Node& shape = *document.graph().findNode("shape");

    auto malformed = publicationFor(shape, "not-json", 2);
    REQUIRE(commands.publishCurveState(malformed).code == GraphEditCode::InvalidTypedSnapshot);
    auto incomplete = publicationFor(shape, modelSnapshotForNode(shape), 1);
    incomplete.controls.pop_back();
    REQUIRE(commands.publishCurveState(incomplete).code == GraphEditCode::InvalidControlValue);
    CurveNodeStatePublication wrongKind {
        "add", 0, shape.model, {}
    };
    REQUIRE(commands.publishCurveState(wrongKind).code == GraphEditCode::WrongNodeKind);

    REQUIRE_FALSE(commands.setNodeParameter(
            "shape", "curve.modelSnapshot", "Curve Model Snapshot", "not-json").succeeded());
}

TEST_CASE("Typed curve snapshots build deterministic immutable DSP data",
        "[cycle-v2][curve-model][dsp]") {
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 1, 0.f, 0.1f, 1.f },
            { 2, 0.5f, 0.8f, 0.5f },
            { 3, 1.f, 0.9f, 1.f }
    }));
    const std::vector<CycleV2::NodeParameter> typedParameters;
    const auto typedModel = CurveNodeModelState::copyOf(model, model.revision());

    const auto firstWaveshaper = WaveshaperSignalProcessor::buildConfiguration(typedParameters, typedModel);
    const auto secondWaveshaper = WaveshaperSignalProcessor::buildConfiguration(typedParameters, typedModel);
    float firstSamples[] { -1.f, -0.5f, 0.f, 0.5f, 1.f };
    float secondSamples[] { -1.f, -0.5f, 0.f, 0.5f, 1.f };
    firstWaveshaper->transfer->process({ firstSamples, 5 }, 1.f, 1.f);
    secondWaveshaper->transfer->process({ secondSamples, 5 }, 1.f, 1.f);
    REQUIRE(std::vector<float>(firstSamples, firstSamples + 5)
            == std::vector<float>(secondSamples, secondSamples + 5));

    const auto firstIr = IrSignalProcessor::buildConfiguration(typedParameters, typedModel);
    const auto secondIr = IrSignalProcessor::buildConfiguration(typedParameters, typedModel);
    REQUIRE(firstIr->impulse == secondIr->impulse);
}

TEST_CASE("Typed Envelope DSP configuration owns independent mesh and rasterizer state",
        "[cycle-v2][curve-model][dsp][envelope]") {
    EnvelopeNodeModel model;
    model.red = 0.2f;
    model.blue = 0.8f;
    const std::vector<CycleV2::NodeParameter> parameters {
            { "red", "Red", "0.2" },
            { "blue", "Blue", "0.8" },
            { "level", "Level", "1" }
    };

    const auto typedModel = CurveNodeModelState::copyOf(model, model.revision());
    const auto first = EnvelopeSignalProcessor::buildConfiguration(parameters, typedModel);
    const auto second = EnvelopeSignalProcessor::buildConfiguration(parameters, typedModel);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(first->mesh.get() != second->mesh.get());
    REQUIRE(first->rasterizer.get() != second->rasterizer.get());
}

TEST_CASE("Malformed typed curve state is rejected by the domain codec",
        "[cycle-v2][curve-model][dsp]") {
    CurveNodeDomainCodec codec(NodeKind::Waveshaper);
    String error;
    REQUIRE(codec.readJSON(JSON::parse("{\"schema\":\"flatCurve\",\"version\":1,\"revision\":1,\"state\":{}}"), error) == nullptr);
    REQUIRE(error.isNotEmpty());
}

TEST_CASE("Curve node definitions provide canonical typed defaults",
        "[cycle-v2][curve-model][defaults]") {
    GraphNodeFactory factory;
    for (const NodeKind kind : {
            NodeKind::Waveshaper,
            NodeKind::ImpulseResponse,
            NodeKind::GuideCurve,
            NodeKind::Envelope }) {
        const Node node = factory.createNode(kind, "curve", {});
        REQUIRE(node.model != nullptr);
        REQUIRE(node.model->revision() == 1);
        const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
        REQUIRE(typed != nullptr);
        if (kind == NodeKind::Envelope) {
            REQUIRE(typed->envelope() != nullptr);
            REQUIRE(typed->envelope()->revision() == 1);
        } else {
            REQUIRE(typed->flatCurve() != nullptr);
            REQUIRE(typed->flatCurve()->revision() == 1);
        }
        REQUIRE(parameterValueForNode(node, "effect.vertices").isEmpty());
        REQUIRE(parameterValueForNode(node, "envelope.snapshot").isEmpty());
    }
}

TEST_CASE("Repository Cycle V2 presets contain canonical typed curve state",
        "[cycle-v2][curve-state][presets]") {
    for (const String& filename : { String("default.cyclegraph"), String("with-spies.cyclegraph") }) {
        const File file = File(CYCLE_V2_SOURCE_DIR).getChildFile("resources").getChildFile(filename);
        const NodeGraph graph = GraphSerializer().fromJsonString(file.loadFileAsString());
        REQUIRE_FALSE(graph.getNodes().empty());
        for (const auto& node : graph.getNodes()) {
            if (node.kind != NodeKind::Envelope
                    && node.kind != NodeKind::GuideCurve
                    && node.kind != NodeKind::ImpulseResponse
                    && node.kind != NodeKind::Waveshaper) {
                continue;
            }
            const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
            REQUIRE(typed != nullptr);
            const uint64_t revision = typed->revision();
            if (node.kind == NodeKind::Envelope) {
                EnvelopeNodeModel model;
                REQUIRE(typed->envelope() != nullptr);
                REQUIRE(model.copyFrom(*typed->envelope()));
                REQUIRE(model.revision() == revision);
                const String canonical = model.snapshot();
                REQUIRE(model.loadSnapshot(canonical));
                REQUIRE(model.snapshot() == canonical);
            } else {
                FlatCurveModel model;
                REQUIRE(typed->flatCurve() != nullptr);
                REQUIRE(model.copyFrom(*typed->flatCurve()));
                REQUIRE(model.revision() == revision);
                const String canonical = model.snapshot();
                REQUIRE(model.loadSnapshot(canonical));
                REQUIRE(model.snapshot() == canonical);
            }
        }
    }
}

TEST_CASE("Loaded typed models require no JSON decoding during runtime consumption",
        "[cycle-v2][curve-model][runtime-boundary]") {
    NodeModelDecodeDiagnostics::reset();
    Mesh::resetJsonReadCount();
    const File file = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(file.loadFileAsString());
    REQUIRE(loaded.succeeded());
    REQUIRE(NodeModelDecodeDiagnostics::count() > 0);
    REQUIRE(Mesh::getJsonReadCount() > 0);
    NodeModelDecodeDiagnostics::reset();
    Mesh::resetJsonReadCount();

    for (const auto& node : loaded.graph.getNodes()) {
        if (node.kind == NodeKind::TrilinearMesh) {
            TrimeshNodeModel model;
            model.syncFromNode(node);
        } else if (node.kind == NodeKind::Envelope) {
            EnvelopePanelAdapter adapter;
            REQUIRE(adapter.syncFromNode(node));
        } else if (node.kind == NodeKind::GuideCurve
                || node.kind == NodeKind::ImpulseResponse
                || node.kind == NodeKind::Waveshaper) {
            FlatCurvePanelAdapter adapter(node.kind);
            REQUIRE(adapter.syncFromNode(node));
        }
    }

    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());
    const auto audio = GraphAudioExecutor().process(loaded.graph, compiled.plan, 64);
    GraphPreviewExecutor().render(compiled.plan, audio, 32);
    REQUIRE(NodeModelDecodeDiagnostics::count() == 0);
    REQUIRE(Mesh::getJsonReadCount() == 0);
}
