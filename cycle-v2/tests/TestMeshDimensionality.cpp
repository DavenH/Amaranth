#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditor.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/MeshDimensionality.h"
#include "Nodes/Control/ModulationTriple.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Runtime/MorphInputs.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "UI/Editors/MeshDimensionOptions.h"
#include "UI/Editors/MorphControlLayout.h"
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>
#include <Curve/Curve.h>

using namespace CycleV2;

namespace {
Node authoredMesh() {
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("DimensionsTest");
    node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
    for (auto& parameter : node.parameters) {
        if (parameter.id == "dimensionCount") {
            parameter.value = "3";
        }
    }
    mesh->destroy();
    return node;
}

float amplitude(const Mesh& mesh, const MorphPosition& morph, int count = 3) {
    VertCube::ReductionData data;
    REQUIRE(Rasterization::TrilinearMeshSlicer().slice(
            *mesh.getCubes().front(), Vertex::Time, data, morph, count));
    VertCube::vertexAt(morph.time, Vertex::Time, &data.v0, &data.v1, &data.v);
    return data.v.values[Vertex::Amp];
}
}

TEST_CASE("Dimensional projection retains Yellow variation and expansion is neutral",
        "[cycle-v2][mesh-dimensions]") {
    Node node = authoredMesh();
    const auto original = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    NodeGraph graph;
    graph.addNode(node);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const auto position = MeshDimensionality::position(node);
    REQUIRE(commands.setMeshDimensions("mesh", 1).succeeded());
    const auto reduced = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    for (const float yellow : { 0.f, 0.25f, 0.8f, 1.f }) {
        const float expected = amplitude(original->mesh(), position.withTime(yellow));
        for (const float red : { 0.f, 0.5f, 1.f }) {
            for (const float blue : { 0.f, 1.f }) {
                REQUIRE(amplitude(reduced->mesh(), { yellow, red, blue }) == Catch::Approx(expected));
                REQUIRE(amplitude(reduced->mesh(), { yellow, red, blue }, 1) == Catch::Approx(expected));
            }
        }
    }
    REQUIRE(commands.setMeshDimensions("mesh", 3).succeeded());
    REQUIRE(document.graph().findNode("mesh")->model == reduced);
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("mesh")->model == original);
}

TEST_CASE("Legacy envelope migration projects Yellow zero and retains markers",
        "[cycle-v2][mesh-dimensions]") {
    EnvelopeNodeModel model;
    auto* first = model.getMesh().getCubes().front();
    for (int corner = 0; corner < 8; ++corner) {
        first->getVertex(corner)->values[Vertex::Amp] = corner >= 4 ? 0.9f : 0.2f;
    }
    Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    node.model = CurveNodeModelState::copyOf(model, 0.5f, 0.5f, 1);
    NodeGraph graph;
    graph.addNode(node);
    auto encoded = GraphSerializer().writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 7);
    auto loaded = GraphSerializer().readJSON(encoded);
    REQUIRE(loaded.succeeded());
    const Node& migrated = *loaded.graph.findNode("env");
    REQUIRE(MeshDimensionality::dimensions(migrated).axisCount() == 2);
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(migrated.model);
    REQUIRE(typed != nullptr);
    const auto& mesh = typed->envelope()->getMesh();
    REQUIRE(mesh.sustainCubes.size() == model.getMesh().sustainCubes.size());
    for (const float yellow : { 0.f, 0.5f, 1.f }) {
        REQUIRE(amplitude(mesh, { yellow, 0.3f, 0.7f }) == Catch::Approx(0.2f));
    }
    auto reloaded = GraphSerializer().readJSON(GraphSerializer().writeJSON(loaded.graph));
    REQUIRE(reloaded.succeeded());
}

TEST_CASE("Morph profiles and unmapped inputs preserve independent fallbacks",
        "[cycle-v2][mesh-dimensions]") {
    const auto defaults = buildModulationTripleConfiguration({});
    REQUIRE(defaults->source(0).mode == ModulationSourceMode::VoiceTime);
    REQUIRE(defaults->source(0, true).mode == ModulationSourceMode::Unmapped);
    REQUIRE(defaults->source(1, true).mode == ModulationSourceMode::KeyScale);
    REQUIRE(defaults->source(2, true).mode == ModulationSourceMode::ModWheel);
    MorphInputs inputs;
    inputs.absoluteOverrides = { 0.8f, 0.9f, 0.1f };
    inputs.hasAbsoluteOverride = { true, false, true };
    const MorphPosition fallback { 0.2f, 0.3f, 0.4f };
    const auto full = inputs.resolve(fallback, MorphDimensions(3), 0);
    REQUIRE((float) full.time == Catch::Approx(0.8f));
    REQUIRE((float) full.red == Catch::Approx(0.3f));
    REQUIRE((float) full.blue == Catch::Approx(0.1f));
    const auto staticMorph = inputs.resolve(fallback, MorphDimensions(0), 0);
    REQUIRE((float) staticMorph.time == Catch::Approx(0.2f));
    REQUIRE((float) staticMorph.blue == Catch::Approx(0.4f));
}

TEST_CASE("New nodes are static and inactive cables remain durable but do not execute",
        "[cycle-v2][mesh-dimensions]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ModulationTriple, "mod", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(MeshDimensionality::dimensions(*graph.findNode("mesh")).axisCount() == 0);
    REQUIRE(MeshDimensionality::dimensions(*graph.findNode("env")).axisCount() == 0);
    graph.addEdge({ "mod", "red", "mesh", "red", PortDomain::ControlSignal });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(compiled.plan.signalEdges.empty());
    const int source = compiled.plan.dependencyIndex.nodeIndexById.at("mod");
    REQUIRE(compiled.plan.dependencyIndex.dependents[(size_t) source].empty());
    const auto inactiveEdit = GraphNodeStateEditor().setNodeParameter(graph, "mesh", "red", "Red", "0.8");
    REQUIRE(inactiveEdit.succeeded());
    REQUIRE(inactiveEdit.changes.parameterImpacts == ParameterImpact::Presentation);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    REQUIRE_FALSE(commands.setNodeParameter("mesh", "dimensionCount", "Dimensions", "bad").succeeded());
    REQUIRE(commands.setMeshDimensions("mesh", 3).succeeded());
    const auto reactivated = GraphCompiler().compile(document.graph());
    REQUIRE(reactivated.succeeded());
    REQUIRE(std::any_of(reactivated.plan.signalEdges.begin(), reactivated.plan.signalEdges.end(),
            [](const Edge& edge) { return edge.sourceNodeId == "mod" && edge.destPortId == "red"; }));
    REQUIRE(document.undo());
    REQUIRE(document.graph().getEdges().size() == 1);
}

TEST_CASE("Reduced vertex deltas edit one logical corner and undo all replicas",
        "[cycle-v2][mesh-dimensions]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("LogicalCorners");
    MorphDimensions::project(*mesh, MorphDimensions(1), { 0.4f, 0.3f, 0.7f });
    const float before = mesh->getVerts().front()->values[Vertex::Amp];
    const auto delta = TrimeshVertexEditCore::prepareVertexValue(*mesh, 0, "vertex.amp", 0.65f, 1);
    REQUIRE(delta.has_value());
    REQUIRE(TrimeshVertexEditCore::apply(*mesh, *delta));
    for (int corner = 0; corner < 4; ++corner) {
        REQUIRE(mesh->getCubes().front()->getVertex(corner)->values[Vertex::Amp] == Catch::Approx(0.65f));
    }
    REQUIRE(TrimeshVertexEditCore::apply(*mesh, delta->inverse()));
    for (int corner = 0; corner < 4; ++corner) {
        REQUIRE(mesh->getCubes().front()->getVertex(corner)->values[Vertex::Amp] == Catch::Approx(before));
    }
    mesh->destroy();
}

TEST_CASE("Static oscillator geometry is never sliced during ordinary or chained playback",
        "[cycle-v2][mesh-dimensions]") {
    Curve::calcTable();
    auto mesh = TrimeshMeshFactory::createDefaultMesh("StaticSlice");
    const MorphPosition morph { 0.4f, 0.3f, 0.7f };
    MorphDimensions::project(*mesh, MorphDimensions(0), morph);
    const auto prepared = Rasterization::PreparedMeshSlice::forMesh(*mesh, morph, Vertex::Time);
    Rasterization::VoiceRasterizer rasterizer;
    Rasterization::VoiceCycleState state;
    rasterizer.prepare(Rasterization::VoiceRasterizerPreparation::forMesh(*mesh), { &state });
    rasterizer.setDimensionality(0, &prepared);
    rasterizer.setMorphPosition(morph);
    rasterizer.setState(&state);
    rasterizer.setMesh(mesh.get());
    rasterizer.setCalcDepthDimensions(false);
    rasterizer.setPrepareIntegrals(false);
    rasterizer.setInterceptPadding(0.02f);
    rasterizer.renderChained(0.2f);
    for (int cycle = 0; cycle < 4; ++cycle) {
        REQUIRE(rasterizer.renderOrdinary(mesh.get(), 0.f).sampleable);
        REQUIRE(rasterizer.renderChained(0.2f).sampleable);
    }
    REQUIRE(rasterizer.diagnostics().sliceCount == 0);
    REQUIRE(rasterizer.diagnostics().capacityFailureCount == 0);
    mesh->destroy();
}

TEST_CASE("Envelope Yellow is latched at the note-on offset and Static skips materialization",
        "[cycle-v2][mesh-dimensions]") {
    EnvelopeNodeModel model;
    for (auto* cube : model.getMesh().getCubes()) {
        for (int corner = 0; corner < 8; ++corner) {
            auto* vertex = cube->getVertex(corner);
            vertex->values[Vertex::Time] = corner >= 4 ? 1.f : 0.f;
            vertex->values[Vertex::Amp] = corner >= 4 ? 0.8f : 0.2f;
        }
    }
    const auto state = CurveNodeModelState::copyOf(model, 0.5f, 0.5f, 1);
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(
            { { "dimensionCount", "Dimensions", "3" } }, state);
    REQUIRE(configuration != nullptr);
    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "yellow-latch", configuration });
    AudioExecutionSpec spec;
    spec.sampleRate = 48000.;
    spec.maximumFrameCount = 8;
    processor.prepareExecution(spec);
    const auto render = [&](bool noteOn, float yellow) {
        AudioProcessContext context;
        context.frameCount = 8;
        context.captureTraversalGrid = false;
        context.timing.sampleRate = spec.sampleRate;
        SignalPayload yellowInput;
        yellowInput.block.samples = std::vector<float> { 0.f, 0.f, 0.f, yellow, yellow, yellow, yellow, yellow };
        context.inputs.resize(3);
        context.inputs[2] = std::move(yellowInput);
        context.outputPorts = { { "env", PortDomain::ControlSignal, ChannelLayout::Mono } };
        if (noteOn) {
            context.voice.events.push_back({ NoteLifecycleType::NoteOn, 3, 0 });
        }
        processor.process(context);
        return context.outputs.front().block.samples;
    };
    const auto first = render(true, 1.f);
    REQUIRE(first[0] == 0.f);
    REQUIRE(first[3] == Catch::Approx(0.8f).margin(0.002f));
    REQUIRE(processor.realtimePreparationDiagnostics().attempts == 1);
    const auto held = render(false, 0.f);
    REQUIRE(held[3] == Catch::Approx(0.8f).margin(0.002f));
    REQUIRE(processor.realtimePreparationDiagnostics().attempts == 1);
    const auto next = render(true, 0.f);
    REQUIRE(next[3] == Catch::Approx(0.2f).margin(0.002f));

    Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "static", {});
    const auto staticConfiguration = EnvelopeSignalProcessor::buildConfiguration(node.parameters, node.model);
    REQUIRE(staticConfiguration != nullptr);
    processor.adoptConfiguration({ 2, "static", staticConfiguration });
    processor.prepareExecution(spec);
    render(true, 1.f);
    REQUIRE(processor.realtimePreparationDiagnostics().attempts == 0);
}

TEST_CASE("Compiler separates morph profiles, shares equal mappings, and refreshes their identity",
        "[cycle-v2][mesh-dimensions]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    for (const auto kind : { NodeKind::VoiceContext, NodeKind::ModulationTriple,
                NodeKind::TrilinearMesh, NodeKind::Envelope, NodeKind::ModulationSource }) {
        const String id = kind == NodeKind::VoiceContext ? "voice"
                : kind == NodeKind::ModulationTriple ? "triple"
                : kind == NodeKind::TrilinearMesh ? "mesh"
                : kind == NodeKind::Envelope ? "env" : "explicit";
        graph.addNode(factory.createNode(kind, id, {}));
    }
    const auto set = [&](const String& node, const String& parameter, const String& value) {
        REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, node, parameter, parameter, value).succeeded());
    };
    set("mesh", "dimensionCount", "3");
    set("env", "dimensionCount", "3");
    set("env", "purpose", "pitch");
    set("triple", "envelopeRedSource", "constant");
    set("triple", "envelopeRedConstant", "0.7");
    set("triple", "envelopeBlueController", "42");
    set("triple", "envelopeBlueConstant", "0.1");
    REQUIRE(GraphEditor().connect(graph,
            { "triple", "modulation", false }, { "voice", "modulation", true }).succeeded());
    REQUIRE(GraphEditor().connect(graph,
            { "env", "env", false }, { "voice", "pitch", true }).succeeded());
    GraphCompiler compiler;
    auto compiled = compiler.compile(graph);
    REQUIRE(compiled.succeeded());
    const auto input = [&](const GraphExecutionPlan& plan, const String& node, const String& port) {
        for (const auto& step : plan.steps) {
            if (step.nodeId == node) {
                for (const auto& candidate : step.inputs) {
                    if (candidate.destPortId == port) {
                        return candidate.sourceBufferIndex;
                    }
                }
            }
        }
        return -1;
    };
    REQUIRE(input(compiled.plan, "env", "yellow") == -1);
    const int envelopeRed = input(compiled.plan, "env", "red");
    REQUIRE(envelopeRed >= 0);
    REQUIRE(envelopeRed != input(compiled.plan, "mesh", "red"));
    REQUIRE(input(compiled.plan, "env", "blue") == input(compiled.plan, "mesh", "blue"));
    auto& buffer = compiled.plan.buffers[(size_t) envelopeRed];
    REQUIRE(buffer.envelopeModulationProfile);
    set("triple", "envelopeRedConstant", "0.4");
    compiler.refreshVoiceContexts(graph, compiled.plan);
    const auto refreshed = std::dynamic_pointer_cast<const ModulationTripleConfiguration>(buffer.defaultModulation);
    REQUIRE(refreshed != nullptr);
    REQUIRE(refreshed->source(1, buffer.envelopeModulationProfile).constant == Catch::Approx(0.4f));
    REQUIRE(refreshed->source(1).mode == ModulationSourceMode::KeyScale);
    REQUIRE(GraphEditor().connect(graph,
            { "explicit", "value", false }, { "env", "red", true }).succeeded());
    compiled = compiler.compile(graph);
    REQUIRE(compiled.succeeded());
    const int explicitBuffer = input(compiled.plan, "env", "red");
    REQUIRE(explicitBuffer >= 0);
    REQUIRE(compiled.plan.buffers[(size_t) explicitBuffer].sourceNodeId == "explicit");
    REQUIRE(compiled.plan.buffers[(size_t) explicitBuffer].defaultModulationSlot == DefaultModulationSlot::None);
}

TEST_CASE("Every dimensional transition supports undo, redo, and reload",
        "[cycle-v2][mesh-dimensions]") {
    for (const auto kind : { NodeKind::TrilinearMesh, NodeKind::Envelope }) {
        for (int before = 0; before <= 3; ++before) {
            for (int after = 0; after <= 3; ++after) {
                Node node = kind == NodeKind::Envelope
                        ? GraphNodeFactory().createNode(kind, "mesh", {}) : authoredMesh();
                NodeGraph graph;
                graph.addNode(std::move(node));
                GraphDocument document(std::move(graph));
                GraphCommandDispatcher commands(document);
                REQUIRE(commands.setMeshDimensions("mesh", before).succeeded());
                const auto original = document.graph().findNode("mesh")->model;
                REQUIRE(commands.setMeshDimensions("mesh", after).succeeded());
                const auto changed = document.graph().findNode("mesh")->model;
                REQUIRE(MeshDimensionality::dimensions(*document.graph().findNode("mesh")).axisCount() == after);
                auto reloaded = GraphSerializer().readJSON(GraphSerializer().writeJSON(document.graph()));
                REQUIRE(reloaded.succeeded());
                REQUIRE(MeshDimensionality::dimensions(*reloaded.graph.findNode("mesh")).axisCount() == after);
                if (before != after) {
                    REQUIRE(document.undo());
                    REQUIRE(document.graph().findNode("mesh")->model == original);
                    REQUIRE(document.redo());
                    REQUIRE(document.graph().findNode("mesh")->model == changed);
                }
            }
        }
    }
}

TEST_CASE("Reduced slicing honors cell boundaries and inactive coordinates cannot be edited",
        "[cycle-v2][mesh-dimensions]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("PartialMorphCells");
    for (auto* vertex : mesh->getVerts()) {
        vertex->values[Vertex::Blue] *= 0.5f;
    }
    MorphDimensions::project(*mesh, MorphDimensions(2), { 0.4f, 0.3f, 0.2f });
    VertCube::ReductionData full;
    VertCube::ReductionData reduced;
    Rasterization::TrilinearMeshSlicer slicer;
    for (const int axis : { Vertex::Time, Vertex::Red, Vertex::Blue }) {
        for (const float blue : { 0.f, 0.2f, 0.5f, 0.8f, 1.f }) {
            const MorphPosition position { 0.4f, 0.3f, blue };
            REQUIRE(slicer.slice(*mesh->getCubes().front(), axis, full, position)
                    == slicer.slice(*mesh->getCubes().front(), axis, reduced, position, 2));
        }
    }
    REQUIRE_FALSE(TrimeshVertexEditCore::prepareVertexValue(*mesh, 0, "vertex.blue", 0.3f, 2).has_value());
    MorphDimensions::project(*mesh, MorphDimensions(0), { 0.4f, 0.3f, 0.8f });
    REQUIRE(mesh->getCubes().empty());
    REQUIRE(mesh->getVerts().empty());
    mesh->destroy();
}

TEST_CASE("Projection remaps component guides and cancellation restores geometry and mappings",
        "[cycle-v2][mesh-dimensions]") {
    Node node = authoredMesh();
    const auto original = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    Mesh mesh;
    mesh.deepCopy(&original->mesh());
    REQUIRE(mesh.getNumCubes() > 1);
    for (auto* vertex : mesh.getCubes().front()->lineVerts) {
        vertex->values[Vertex::Blue] *= 0.5f;
    }
    node.model = TrimeshNodeModelState::copyOf(mesh, 2);
    mesh.destroy();
    for (auto& parameter : node.parameters) {
        if (parameter.id == "blue") {
            parameter.value = "0.8";
        }
    }
    NodeGraph graph;
    graph.addNode(node);
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({ { 1, 0.05f, 0.6f, 1.f }, { 2, 0.95f, 0.6f, 1.f } }));
    GuideCurveResource guide;
    guide.id = "guide";
    guide.shortLabel = "G1";
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({ "guide", "mesh", { 1, GuideCurveField::Time } }));
    REQUIRE(graph.assignGuideCurve({ "guide", "mesh", { 1, GuideCurveField::Red } }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    commands.beginCompoundEdit();
    REQUIRE(commands.setMeshDimensions("mesh", 0).succeeded());
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
    REQUIRE(document.graph().getGuideAssignments().front().target.cubeIndex == 0);
    REQUIRE(document.graph().getGuideAssignments().front().target.field == GuideCurveField::Time);
    commands.cancelCompoundEdit();
    REQUIRE(document.graph().findNode("mesh")->model == node.model);
    REQUIRE(document.graph().getGuideAssignments().size() == 2);
    REQUIRE(commands.setMeshDimensions("mesh", 0).succeeded());
    REQUIRE(document.undo());
    REQUIRE(document.graph().getGuideAssignments().size() == 2);
    REQUIRE(document.graph().getGuideAssignments().front().target.cubeIndex == 1);
    REQUIRE(document.redo());
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
    REQUIRE(document.graph().getGuideAssignments().front().target.cubeIndex == 0);
}

TEST_CASE("Static Scratch envelopes still advance through playback time without live slices",
        "[cycle-v2][mesh-dimensions]") {
    Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "scratch", {});
    for (auto& parameter : node.parameters) {
        if (parameter.id == "purpose") {
            parameter.value = "scratch";
        }
    }
    const auto configuration = EnvelopeSignalProcessor::buildConfiguration(node.parameters, node.model);
    REQUIRE(configuration != nullptr);
    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "static-scratch", configuration });
    AudioExecutionSpec spec;
    spec.sampleRate = 64.;
    spec.maximumFrameCount = 64;
    processor.prepareExecution(spec);
    AudioProcessContext context;
    context.frameCount = 64;
    context.timing.sampleRate = 64.;
    context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
    context.voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    processor.process(context);
    const auto& samples = context.outputs.front().block.samples;
    const auto range = std::minmax_element(samples.begin(), samples.end());
    REQUIRE(*range.second - *range.first > 0.3f);
    REQUIRE(processor.realtimePreparationDiagnostics().attempts == 0);
}

TEST_CASE("Reduced interpolation matches mature evaluation across domains and boundaries",
        "[cycle-v2][mesh-dimensions]") {
    for (const int count : { 1, 2 }) {
        auto mesh = TrimeshMeshFactory::createDefaultMesh("ReducedParity");
        MorphDimensions::project(*mesh, MorphDimensions::trimesh(count), { 0.3f, 0.4f, 0.5f });
        for (const auto domain : { PortDomain::TimeSignal, PortDomain::SpectralMagnitudeSignal,
                    PortDomain::SpectralPhaseSignal }) {
            for (const float position : { 0.f, 0.27f, 0.75f, 1.f }) {
                const MorphPosition morph { position, position, position };
                const int axis = MorphDimensions::trimesh(count).primaryAxis();
                TrimeshBlockwiseDsp mature;
                TrimeshBlockwiseDsp reduced;
                reduced.setDimensionality(count);
                mature.prepareSampling(32);
                reduced.prepareSampling(32);
                mature.prepare(mesh.get(), morph, axis, domain == PortDomain::TimeSignal, domain);
                reduced.prepare(mesh.get(), morph, axis, domain == PortDomain::TimeSignal, domain);
                float expected[32];
                float actual[32];
                mature.renderCycleInto({ expected, 32 }, domain);
                reduced.renderCycleInto({ actual, 32 }, domain);
                for (int sample = 0; sample < 32; ++sample) {
                    REQUIRE(actual[sample] == Catch::Approx(expected[sample]).margin(0.00001f));
                }
            }
        }
        mesh->destroy();
    }
}

TEST_CASE("Migrated Envelope retains guided sustain and release across Red and Blue",
        "[cycle-v2][mesh-dimensions]") {
    EnvelopeNodeModel model;
    model.getMesh().loopCubes.insert(model.getMesh().getCubes()[1]);
    Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    node.model = CurveNodeModelState::copyOf(model, 0.5f, 0.5f, 1);
    NodeGraph graph;
    graph.addNode(node);
    FlatCurveModel guideModel;
    REQUIRE(guideModel.replaceVertices({ { 1, 0.f, 0.4f, 1.f }, { 2, 1.f, 0.6f, 1.f } }));
    GuideCurveResource guide;
    guide.id = "guide";
    guide.shortLabel = "G1";
    guide.model = CurveNodeModelState::copyOf(guideModel, 2);
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    for (const auto field : { GuideCurveField::Time, GuideCurveField::Red, GuideCurveField::Blue }) {
        REQUIRE(graph.assignGuideCurve({ "guide", "env", { 2, field }, GuideCurveTargetKind::EnvelopeCubeComponent }));
    }
    auto encoded = GraphSerializer().writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 7);
    const auto migrated = GraphSerializer().readJSON(encoded);
    String migrationIssues;
    for (const auto& issue : migrated.issues) {
        migrationIssues << issue.message << "; ";
    }
    INFO(migrationIssues);
    REQUIRE(migrated.succeeded());
    const auto render = [&](const NodeGraph& source, int count, float red, float blue) {
        const auto* envelope = source.findNode("env");
        const auto configuration = EnvelopeSignalProcessor::buildConfiguration({
                { "dimensionCount", "Dimensions", String(count) },
                { "red", "Red", String(red) }, { "blue", "Blue", String(blue) }
        }, envelope->model, &source, "env");
        REQUIRE(configuration != nullptr);
        const auto view = configuration->rasterizer->preparedPlaybackView();
        Rasterization::EnvelopePlaybackEngine playback;
        playback.ensureVoiceCount(1);
        playback.validate(view);
        playback.noteOn();
        MeshLibrary::EnvProps props;
        props.active = true;
        std::vector<float> samples;
        for (int sample = 0; sample < 64; ++sample) {
            if (sample == 32) {
                playback.noteOff(view);
            }
            playback.renderToBuffer(view, 1, 0.025, Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex, props, 1.f);
            samples.push_back(playback.sustainLevel(Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex));
        }
        return samples;
    };
    for (const float red : { 0.f, 0.3f, 1.f }) {
        for (const float blue : { 0.f, 0.7f, 1.f }) {
            const auto expected = render(graph, 3, red, blue);
            const auto actual = render(migrated.graph, 2, red, blue);
            REQUIRE(actual.size() == expected.size());
            for (size_t sample = 0; sample < actual.size(); ++sample) {
                REQUIRE(actual[sample] == Catch::Approx(expected[sample]).margin(0.00001f));
            }
        }
    }
}

TEST_CASE("Logical multi-update movement stays local as unrelated geometry grows",
        "[cycle-v2][mesh-dimensions][complexity]") {
    uint64_t expectedVisits = 0;
    for (const int unrelated : { 0, 100 }) {
        auto mesh = TrimeshMeshFactory::createDefaultMesh("LogicalComplexity");
        for (int cell = 0; cell < unrelated; ++cell) {
            TrimeshMeshFactory::addVoiceCube(*mesh, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f);
        }
        MorphDimensions::project(*mesh, MorphDimensions(1), { 0.2f, 0.3f, 0.4f });
        InteractionComplexityDiagnostics::reset();
        const float original = mesh->getVerts().front()->values[Vertex::Amp];
        const auto first = TrimeshVertexEditCore::prepareVertexValue(*mesh, 0, "vertex.amp", 0.6f, 1);
        REQUIRE(first.has_value());
        REQUIRE(TrimeshVertexEditCore::apply(*mesh, *first));
        const auto second = TrimeshVertexEditCore::prepareVertexValue(*mesh, 0, "vertex.amp", 0.8f, 1);
        REQUIRE(second.has_value());
        REQUIRE(TrimeshVertexEditCore::apply(*mesh, *second));
        const auto committed = TrimeshVertexEditCore::compose(*first, *second);
        REQUIRE(committed.has_value());
        REQUIRE(mesh->getVerts().front()->values[Vertex::Amp] == Catch::Approx(0.8f));
        REQUIRE(TrimeshVertexEditCore::apply(*mesh, committed->inverse()));
        REQUIRE(mesh->getVerts().front()->values[Vertex::Amp] == Catch::Approx(original));
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.meshEditOwnerVisits > 0);
        if (unrelated == 0) {
            expectedVisits = counts.meshEditOwnerVisits;
        } else {
            REQUIRE(counts.meshEditOwnerVisits == expectedVisits);
        }
        mesh->destroy();
    }
}

TEST_CASE("Trimesh descends Yellow Red Blue to Yellow Red to Yellow without changing its primary",
        "[cycle-v2][mesh-dimensions]") {
    Node node = authoredMesh();
    const auto original = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    NodeGraph graph;
    graph.addNode(node);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    for (const int count : { 2, 1, 0 }) {
        REQUIRE(commands.setMeshDimensions("mesh", count).succeeded());
        const auto* current = document.graph().findNode("mesh");
        const auto dimensions = MeshDimensionality::dimensions(*current);
        REQUIRE(dimensions.cornerMask() == (count == 2 ? 6 : count == 1 ? 4 : 0));
        REQUIRE(NodeParameterMap(*current).stringValue("primaryAxis", {}) == "yellow");
        if (count == 2) {
            const auto reduced = std::dynamic_pointer_cast<const TrimeshNodeModelState>(current->model);
            const auto authored = MeshDimensionality::position(node);
            for (const float yellow : { 0.f, 0.4f, 1.f }) {
                for (const float red : { 0.f, 0.6f, 1.f }) {
                    const float expected = amplitude(original->mesh(), { yellow, red, authored.blue });
                    for (const float blue : { 0.f, 0.7f, 1.f }) {
                        REQUIRE(amplitude(reduced->mesh(), { yellow, red, blue }) == Catch::Approx(expected));
                    }
                }
            }
        }
    }
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("mesh")->model == original);
    REQUIRE(commands.setNodeParameter("mesh", "primaryAxis", "Primary Axis", "red").succeeded());
    REQUIRE(commands.setMeshDimensions("mesh", 2).succeeded());
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh")).stringValue("primaryAxis", {}) == "red");
    REQUIRE(commands.setMeshDimensions("mesh", 1).succeeded());
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh")).stringValue("primaryAxis", {}) == "yellow");
}

TEST_CASE("Morphing options identify axes and warn only when variation is dismissed",
        "[cycle-v2][mesh-dimensions][ui]") {
    const auto options = meshDimensionOptions();
    REQUIRE(options[0].ticks.empty());
    REQUIRE(options[1].ticks.size() == 1);
    REQUIRE(options[2].ticks.size() == 2);
    REQUIRE(options[2].ticks[0] == Colour(0xffe0c247));
    REQUIRE(options[2].ticks[1] == Colour(0xffd65a5a));
    REQUIRE(options[3].ticks.size() == 3);
    REQUIRE(meshDimensionTooltip(3, 2).contains("Blue"));
    REQUIRE(meshDimensionTooltip(3, 2).contains("discards"));
    REQUIRE(meshDimensionTooltip(3, 0).contains("Undo"));
    REQUIRE_FALSE(meshDimensionTooltip(1, 2).contains("discards"));
    REQUIRE(meshDimensionTooltip(1, 2, MorphDimensions::Plane::RedBlue).contains("discards"));
    REQUIRE(meshDimensionOptions(MorphDimensions::Plane::RedBlue)[2].ticks[1] == Colour(0xff5f91e8));
}

TEST_CASE("Surviving Trimesh rails and parameters occupy stable slots in every mode",
        "[cycle-v2][mesh-dimensions][ui]") {
    const Rectangle<float> content(20.f, 40.f, 1200.f, 650.f);
    const auto full = TrimeshWidget::expandedControlHitRegions(content, true, "gain", 3);
    for (const int count : { 2, 1, 0 }) {
        const auto reduced = TrimeshWidget::expandedControlHitRegions(content, true, "gain", count);
        for (const auto& region : reduced) {
            const auto before = std::find_if(full.begin(), full.end(), [&](const auto& candidate) {
                return candidate.kind == region.kind && candidate.parameterId == region.parameterId
                        && candidate.axisValue == region.axisValue;
            });
            REQUIRE(before != full.end());
            REQUIRE(region.bounds == before->bounds);
        }
    }
    for (const auto plane : { MorphDimensions::Plane::YellowRed, MorphDimensions::Plane::RedBlue }) {
        const MorphControlLayout fullLayout(content, 3, plane);
        for (const int count : { 1, 2 }) {
            const MorphControlLayout reduced(content, count, plane);
            for (int axis = 0; axis < 3; ++axis) {
                if (!reduced.rail(axis).isEmpty()) {
                    REQUIRE(reduced.rail(axis) == fullLayout.rail(axis));
                }
            }
            REQUIRE(reduced.vertices() == fullLayout.vertices());
        }
    }
}

TEST_CASE("Format eight Red Blue Trimeshes retain their variation when adopting the Yellow Red plane",
        "[cycle-v2][mesh-dimensions][migration]") {
    Node node = authoredMesh();
    auto original = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    Mesh geometry;
    geometry.deepCopy(&original->mesh());
    MorphDimensions::project(geometry, MorphDimensions(2), { 0.f, 0.5f, 0.5f });
    node.model = TrimeshNodeModelState::copyOf(geometry, 2);
    geometry.destroy();
    for (auto& parameter : node.parameters) {
        if (parameter.id == "dimensionCount") {
            parameter.value = "2";
        }
    }
    NodeGraph graph;
    graph.addNode(node);
    auto encoded = GraphSerializer().writeJSON(graph);
    encoded.getDynamicObject()->setProperty("formatVersion", 8);
    const auto loaded = GraphSerializer().readJSON(encoded);
    REQUIRE(loaded.succeeded());
    REQUIRE(MeshDimensionality::dimensions(*loaded.graph.findNode("mesh")).axisCount() == 3);
    const auto migrated = std::dynamic_pointer_cast<const TrimeshNodeModelState>(loaded.graph.findNode("mesh")->model);
    const auto expected = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    for (const float red : { 0.f, 0.7f, 1.f }) {
        for (const float blue : { 0.f, 0.3f, 1.f }) {
            REQUIRE(amplitude(migrated->mesh(), { 0.4f, red, blue })
                    == Catch::Approx(amplitude(expected->mesh(), { 0.4f, red, blue })));
        }
    }
}
