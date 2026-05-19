#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Runtime/GraphPreviewExecutor.h"

#include <algorithm>

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

}

TEST_CASE("Graph preview executor renders previewable compiled nodes", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE_FALSE(result.nodes.empty());
    REQUIRE(findPreview(result, "waveMesh").role == PreviewModuleRole::MeshSurface);
    REQUIRE(findPreview(result, "waveMesh").primary == std::vector<float> { 0.25f, 0.75f, 0.25f, 0.75f });
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

TEST_CASE("Graph preview executor passes parameters to preview processors", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.getNodesForEditing().back().parameters = { { "amplitude", "Amplitude", "0.5" } };

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 3);

    REQUIRE(findPreview(result, "wave").primary == std::vector<float> { 0.f, 0.5f, 0.f });
}

TEST_CASE("Graph preview executor propagates upstream summaries through non-preview nodes", "[cycle-v2][runtime]") {
    NodeGraph graph;
    graph.addNode(previewNode(
            "source",
            NodeKind::GenericProcessor,
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", { 220.f, 0.f }));
    graph.addNode(previewNode(
            "mesh",
            NodeKind::TrilinearMesh,
            { { "time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, true } },
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }));
    graph.addEdge({ "source", "out", "reverb", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "reverb", "time", "mesh", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE(findPreview(result, "mesh").secondary.size() == 4);
    REQUIRE(findPreview(result, "mesh").secondary[0] == Catch::Approx(0.8f));
    REQUIRE(findPreview(result, "mesh").secondary[3] == Catch::Approx(0.2f));
}
