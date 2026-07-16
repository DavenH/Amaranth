#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Runtime/GraphAudioExecutor.h"
#include "../src/Runtime/GraphPreviewExecutor.h"
#include "../src/Runtime/NodePreviewProcessor.h"

#include <algorithm>
#include <cmath>

using namespace CycleV2;

namespace {

const NodePreviewResult& findPreview(const GraphPreviewResult& result, const String& nodeId) {
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [&](const NodePreviewResult& preview) {
                return preview.nodeId == nodeId;
            });

    REQUIRE(found != result.nodes.end());
    return *found;
}

const NodeAudioResult& findAudio(const GraphAudioResult& result, const String& nodeId) {
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [&](const NodeAudioResult& node) {
                return node.nodeId == nodeId;
            });

    REQUIRE(found != result.nodes.end());
    return *found;
}

const SignalPayload& outputForPort(const NodeAudioResult& node, const String& portId) {
    const auto found = std::find_if(
            node.outputs.begin(),
            node.outputs.end(),
            [&](const std::pair<String, SignalPayload>& output) {
                return output.first == portId;
            });

    REQUIRE(found != node.outputs.end());
    return found->second;
}

float absoluteSum(const std::vector<float>& values) {
    float sum = 0.f;

    for (const float value : values) {
        sum += value >= 0.f ? value : -value;
    }

    return sum;
}

float absoluteDifferenceSum(const std::vector<float>& left, const std::vector<float>& right) {
    REQUIRE(left.size() == right.size());

    float sum = 0.f;
    for (size_t i = 0; i < left.size(); ++i) {
        const float diff = left[i] - right[i];
        sum += diff >= 0.f ? diff : -diff;
    }

    return sum;
}

float columnDifference(
        const SignalTraversalGrid& grid,
        size_t leftColumn,
        size_t rightColumn) {
    REQUIRE(grid.isValid());
    REQUIRE(leftColumn < grid.columns);
    REQUIRE(rightColumn < grid.columns);

    float sum = 0.f;
    for (size_t row = 0; row < grid.rows; ++row) {
        const float diff = grid.values[leftColumn * grid.rows + row]
                - grid.values[rightColumn * grid.rows + row];
        sum += diff >= 0.f ? diff : -diff;
    }

    return sum;
}

void requireGridDeltaEquals(
        const SignalTraversalGrid& after,
        const SignalTraversalGrid& before,
        const SignalTraversalGrid& delta) {
    REQUIRE(after.columns == before.columns);
    REQUIRE(after.rows == before.rows);
    REQUIRE(delta.columns == after.columns);
    REQUIRE(delta.rows == after.rows);

    float maxError = 0.f;
    for (size_t i = 0; i < after.columns * after.rows; ++i) {
        const float error = std::abs(after.values[i] - before.values[i] - delta.values[i]);
        maxError = std::max(maxError, error);
    }

    REQUIRE(maxError < 1.0e-5f);
}

void requireMagnitudeGridAddEquals(
        const SignalTraversalGrid& after,
        const SignalTraversalGrid& before,
        const SignalTraversalGrid& delta) {
    REQUIRE(after.columns == before.columns);
    REQUIRE(after.rows == before.rows);
    REQUIRE(delta.columns == after.columns);
    REQUIRE(delta.rows == after.rows);

    float maxError = 0.f;
    for (size_t i = 0; i < after.columns * after.rows; ++i) {
        const float expected = jmax(0.f, before.values[i] + delta.values[i]);
        const float error = std::abs(after.values[i] - expected);
        maxError = std::max(maxError, error);
    }

    REQUIRE(maxError < 1.0e-5f);
}

Node previewNode(String id, NodeKind kind, std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        id,
        kind,
        id,
        {},
        {},
        {},
        std::move(inputs),
        std::move(outputs)
    };
}

NodeGraph previewlessChain(size_t processorCount) {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));

    String sourceId = "wave";
    String sourcePort = "out";
    for (size_t i = 0; i < processorCount; ++i) {
        const String nodeId = "reverb" + String((int) i);
        graph.addNode(factory.createNode(NodeKind::Reverb, nodeId, { (float) i * 100.f, 0.f }));
        graph.addEdge({ sourceId, sourcePort, nodeId, "time", PortDomain::TimeSignal, false });
        sourceId = nodeId;
        sourcePort = "time";
    }

    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 500.f, 0.f }));
    graph.addEdge({ sourceId, sourcePort, "shape", "time", PortDomain::TimeSignal, false });
    return graph;
}

TEST_CASE("Graph preview executor can render spy nodes from audio traversal grids", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "spy", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 760.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "spy", "in", PortDomain::ControlSignal, false });
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto audio = GraphAudioExecutor().process(graph, compileResult.plan, 6);
    const auto result = GraphPreviewExecutor().render(compileResult.plan, audio, 6);
    const auto& meshAudio = findAudio(audio, "mesh");
    const auto& spy = findPreview(result, "spy");

    REQUIRE(meshAudio.output.traversalGrid.isValid());
    REQUIRE(spy.role == PreviewModuleRole::SignalSpy);
    REQUIRE(spy.gridColumns == meshAudio.output.traversalGrid.columns);
    REQUIRE(spy.gridRows == meshAudio.output.traversalGrid.rows);
    REQUIRE(spy.domain == meshAudio.output.traversalGrid.metadata.valueDomain);
    REQUIRE(spy.primary == meshAudio.output.traversalGrid.values);
}

TEST_CASE("Trimesh preview reuses a compatible captured traversal", "[cycle-v2][runtime][complexity]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 0.f, 0.f }));

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());
    const auto audio = GraphAudioExecutor().process(graph, compileResult.plan, 16);

    const auto reused = GraphPreviewExecutor().render(compileResult.plan, audio, 16);
    REQUIRE(reused.reusedCapturedTraversalCount == 1);
    REQUIRE(findPreview(reused, "mesh").gridRows == 16);
    REQUIRE(findPreview(reused, "mesh").gridColumns == 8);

    const auto distinctResolution = GraphPreviewExecutor().render(
            compileResult.plan, audio, 12);
    REQUIRE(distinctResolution.reusedCapturedTraversalCount == 0);
    REQUIRE(findPreview(distinctResolution, "mesh").gridRows == 12);
}

TEST_CASE("Graph preview executor does not invent spy traversal grids without audio data", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "spy", { 520.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "spy", "in", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 6);
    const auto& spy = findPreview(result, "spy");

    REQUIRE(spy.role == PreviewModuleRole::SignalSpy);
    REQUIRE(spy.primary.empty());
    REQUIRE(spy.gridColumns == 0);
    REQUIRE(spy.gridRows == 0);
}

TEST_CASE("Graph preview executor updates mesh spy traversal grids from serialized edits", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph baselineGraph;
    NodeGraph editedGraph;

    auto addGraph = [&factory](NodeGraph& graph, std::vector<NodeParameter> meshParameters) {
        auto mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 0.f, 0.f });
        mesh.parameters = std::move(meshParameters);

        graph.addNode(mesh);
        graph.addNode(factory.createNode(NodeKind::Spy, "spy", { 240.f, 0.f }));
        graph.addEdge({ "mesh", "out", "spy", "in", PortDomain::TimeSignal, false });
    };

    addGraph(baselineGraph, {});
    addGraph(
            editedGraph,
            {
                    { "mesh.vertex.0.amp", "Amplitude", "0.05" },
                    { "mesh.vertex.1.amp", "Amplitude", "0.95" },
                    { "mesh.vertex.2.phase", "Phase", "0.44" }
            });

    const auto baselineCompile = GraphCompiler().compile(baselineGraph);
    const auto editedCompile = GraphCompiler().compile(editedGraph);
    REQUIRE(baselineCompile.succeeded());
    REQUIRE(editedCompile.succeeded());

    const auto baselineAudio = GraphAudioExecutor().process(baselineGraph, baselineCompile.plan, 128);
    const auto editedAudio = GraphAudioExecutor().process(editedGraph, editedCompile.plan, 128);
    const auto baselinePreview = GraphPreviewExecutor().render(baselineCompile.plan, baselineAudio, 40);
    const auto editedPreview = GraphPreviewExecutor().render(editedCompile.plan, editedAudio, 40);
    const auto& baselineMeshPreview = findPreview(baselinePreview, "mesh");
    const auto& editedMeshPreview = findPreview(editedPreview, "mesh");
    const auto& baselineSpy = findPreview(baselinePreview, "spy");
    const auto& editedSpy = findPreview(editedPreview, "spy");

    REQUIRE(absoluteDifferenceSum(editedMeshPreview.primary, baselineMeshPreview.primary) > 0.01f);
    REQUIRE(baselineSpy.primary == findAudio(baselineAudio, "mesh").output.traversalGrid.values);
    REQUIRE(editedSpy.primary == findAudio(editedAudio, "mesh").output.traversalGrid.values);
    REQUIRE(absoluteDifferenceSum(editedSpy.primary, baselineSpy.primary) > 0.01f);
}

TEST_CASE("Graph preview executor renders FFT spy previews from audio traversal grids", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "magSpy", { 520.f, -120.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "phaseSpy", { 520.f, 120.f }));
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "magSpy", "in", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "phaseSpy", "in", PortDomain::SpectralPhaseSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto audio = GraphAudioExecutor().process(graph, compileResult.plan, 128);
    const auto result = GraphPreviewExecutor().render(compileResult.plan, audio, 40);
    const auto& fft = findAudio(audio, "fft");
    const auto& magnitude = outputForPort(fft, "mag");
    const auto& phase = outputForPort(fft, "phase");
    const auto& magSpy = findPreview(result, "magSpy");
    const auto& phaseSpy = findPreview(result, "phaseSpy");

    REQUIRE(magnitude.traversalGrid.isValid());
    REQUIRE(phase.traversalGrid.isValid());
    REQUIRE(absoluteSum(magnitude.traversalGrid.values) > 0.01f);
    REQUIRE(absoluteSum(phase.traversalGrid.values) > 0.01f);
    REQUIRE(magSpy.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(phaseSpy.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(magSpy.gridColumns == magnitude.traversalGrid.columns);
    REQUIRE(magSpy.gridRows == magnitude.traversalGrid.rows);
    REQUIRE(phaseSpy.gridColumns == phase.traversalGrid.columns);
    REQUIRE(phaseSpy.gridRows == phase.traversalGrid.rows);
    REQUIRE(magSpy.primary == magnitude.traversalGrid.values);
    REQUIRE(phaseSpy.primary == phase.traversalGrid.values);
}

TEST_CASE("Graph preview executor renders every spy in the bundled spy graph", "[cycle-v2][runtime]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File spyGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("with-spies.cyclegraph");

    REQUIRE(spyGraph.existsAsFile());

    const NodeGraph graph = GraphSerializer().fromXmlString(spyGraph.loadFileAsString());
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto audio = GraphAudioExecutor().process(graph, compileResult.plan, 128);
    const auto result = GraphPreviewExecutor().render(compileResult.plan, audio, 96);
    const auto& fftMagnitude = outputForPort(findAudio(audio, "fft"), "mag");
    const auto& magMesh = findAudio(audio, "magMesh").output;
    const auto& addMag = findAudio(audio, "addMag").output;
    const auto& addSpy = findPreview(result, "spy6");

    const StringArray spyIds { "spy", "spy2", "spy3", "spy4", "spy5", "spy6", "spy7", "spy8" };
    for (const auto& spyId : spyIds) {
        const auto& spy = findPreview(result, spyId);
        INFO("spy id: " << spyId);
        REQUIRE(spy.role == PreviewModuleRole::SignalSpy);
        REQUIRE(spy.gridColumns > 0);
        REQUIRE(spy.gridRows > 0);
        REQUIRE(spy.primary.size() == spy.gridColumns * spy.gridRows);
        REQUIRE(absoluteSum(spy.primary) > 0.01f);
    }

    REQUIRE(absoluteDifferenceSum(findPreview(result, "spy2").primary, findPreview(result, "spy6").primary) > 0.01f);
    REQUIRE(magMesh.traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(magMesh.traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(columnDifference(magMesh.traversalGrid, 0, magMesh.traversalGrid.columns - 1) > 0.01f);
    REQUIRE(*std::min_element(addMag.traversalGrid.values.begin(), addMag.traversalGrid.values.end()) >= 0.f);
    requireMagnitudeGridAddEquals(addMag.traversalGrid, fftMagnitude.traversalGrid, magMesh.traversalGrid);
    REQUIRE(addSpy.gridColumns == addMag.traversalGrid.columns);
    REQUIRE(addSpy.gridRows == addMag.traversalGrid.rows);
    REQUIRE(absoluteDifferenceSum(addSpy.primary, addMag.traversalGrid.values) < 1.0e-5f);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE(
        "Graph preview address lookup scales with compiled inputs",
        "[cycle-v2][runtime][complexity]") {
    for (const size_t processorCount : { 8u, 16u, 32u }) {
        const auto compileResult = GraphCompiler().compile(previewlessChain(processorCount));
        REQUIRE(compileResult.succeeded());

        const auto result = GraphPreviewExecutor().render(compileResult.plan, 16);
        INFO("processor count: " << processorCount);
        REQUIRE(findPreview(result, "shape").primary.size() == 16);
        REQUIRE(result.addressLookupCount == processorCount + 1);
        REQUIRE(result.aliasedInputCount == processorCount);
    }
}

}

TEST_CASE("Graph preview executor renders previewable compiled nodes", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE_FALSE(result.nodes.empty());
    REQUIRE(findPreview(result, "waveMesh").role == PreviewModuleRole::MeshSurface);
    REQUIRE(findPreview(result, "waveMesh").primary.size() == 32);
    REQUIRE(findPreview(result, "waveMesh").secondary.size() == 4);
    REQUIRE(findPreview(result, "waveMesh").gridColumns == 8);
    REQUIRE(findPreview(result, "waveMesh").gridRows == 4);
    REQUIRE(findPreview(result, "waveMesh").domain == PortDomain::TimeSignal);
    REQUIRE(findPreview(result, "env").role == PreviewModuleRole::Envelope);
    REQUIRE(std::none_of(
            result.nodes.begin(),
            result.nodes.end(),
            [](const NodePreviewResult& preview) {
                return preview.nodeId == "out";
            }));
}

TEST_CASE("Graph preview executor skips non-preview utility nodes", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE(std::none_of(
            result.nodes.begin(),
            result.nodes.end(),
            [](const NodePreviewResult& preview) {
                return preview.nodeId == "fft" || preview.nodeId == "ifft" || preview.nodeId == "multiply";
            }));
}

TEST_CASE("Graph preview executor gives spy nodes upstream audio traversal grids", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "spy", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 760.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "spy", "in", PortDomain::ControlSignal, false });
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto audio = GraphAudioExecutor().process(graph, compileResult.plan, 6);
    const auto result = GraphPreviewExecutor().render(compileResult.plan, audio, 6);
    const auto& mesh = findAudio(audio, "mesh").output;
    const auto& spy = findPreview(result, "spy");

    REQUIRE(mesh.traversalGrid.isValid());
    REQUIRE(spy.role == PreviewModuleRole::SignalSpy);
    REQUIRE(mesh.traversalGrid.columns == spy.gridColumns);
    REQUIRE(mesh.traversalGrid.rows == spy.gridRows);
    REQUIRE(spy.gridColumns > 1);
    REQUIRE(spy.gridRows == 6);
    REQUIRE(spy.domain == mesh.traversalGrid.metadata.valueDomain);
    REQUIRE(spy.primary == mesh.traversalGrid.values);
}

TEST_CASE("Graph preview executor passes parameters to preview processors", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.replaceNodeParameters("wave", { { "amplitude", "Amplitude", "0.5" } });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 3);

    REQUIRE(findPreview(result, "wave").primary == std::vector<float> { 0.f, 0.5f, 0.f });
}

TEST_CASE("Mesh preview processor uses non-cyclic rendering for spectral outputs", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    auto timeProcessor = factory.create(PreviewModuleRole::MeshSurface);
    auto spectralProcessor = factory.create(PreviewModuleRole::MeshSurface);

    PreviewProcessContext timeContext;
    timeContext.pointCount = 12;
    timeContext.outputPorts = {
            { "out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo }
    };

    PreviewProcessContext spectralContext;
    spectralContext.pointCount = 12;
    spectralContext.outputPorts = {
            { "out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo }
    };

    timeProcessor->render(timeContext);
    spectralProcessor->render(spectralContext);

    REQUIRE(timeContext.secondary.size() == spectralContext.secondary.size());
    REQUIRE_FALSE(timeContext.secondary.empty());
    REQUIRE(timeContext.secondary != spectralContext.secondary);
    REQUIRE(timeContext.primary != spectralContext.primary);
}

TEST_CASE("Graph preview executor renders preview nodes after non-preview processors", "[cycle-v2][runtime]") {
    NodeGraph graph;
    graph.addNode(previewNode(
            "source",
            NodeKind::GenericProcessor,
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", { 220.f, 0.f }));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Waveshaper, "waveshaper", { 440.f, 0.f }));
    graph.addEdge({ "source", "out", "reverb", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "reverb", "time", "waveshaper", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE(findPreview(result, "waveshaper").secondary.size() == 4);
    REQUIRE(findPreview(result, "waveshaper").secondary[0] == Catch::Approx(0.f));
    REQUIRE(findPreview(result, "waveshaper").secondary[3] == Catch::Approx(1.f));
}
