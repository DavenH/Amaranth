#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
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

}

TEST_CASE("Graph preview executor renders previewable compiled nodes", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphPreviewExecutor().render(compileResult.plan, 4);

    REQUIRE_FALSE(result.nodes.empty());
    REQUIRE(findPreview(result, "waveMesh").role == PreviewModuleRole::MeshSurface);
    REQUIRE(findPreview(result, "waveMesh").primary == std::vector<float> { 0.25f, 0.75f, 0.25f, 0.75f });
    REQUIRE(findPreview(result, "env").role == PreviewModuleRole::Envelope);
    REQUIRE(findPreview(result, "out").role == PreviewModuleRole::OutputMeters);
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
