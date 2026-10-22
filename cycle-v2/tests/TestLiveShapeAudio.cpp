#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphSerializer.h"
#include "Graph/TrimeshAuthoringEdits.h"
#include "Runtime/OfflineGraphAudioRenderer.h"
#include "TrimeshAuthoringTestSupport.h"

using namespace CycleV2;

TEST_CASE("An addressed live shape edit changes seeded audio and undo restores exact PCM",
        "[cycle-v2][live-shape][audio]") {
    auto graph = TestSupport::authoredTrimeshGraph(0);
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "mesh", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    OfflineGraphAudioRequest request;
    request.sampleRate = 44100.;
    request.blockSize = 173;
    request.sampleCount = 8192;
    request.hasRandomSeed = true;
    request.randomSeed = 7143;
    request.events = { { 0, MidiMessage::noteOn(1, 60, (uint8) 100) } };
    const auto render = [&] {
        const auto compiled = GraphCompiler().compile(document.graph());
        REQUIRE(compiled.succeeded());
        auto result = OfflineGraphAudioRenderer::render(compiled.plan, document.revision(), request);
        REQUIRE(result.succeeded);
        return result.channels;
    };
    const auto baseline = render();
    TrimeshAuthoringEdits edits;
    edits.nodeId = "mesh";
    edits.expectedDocumentRevision = document.revision();
    edits.expectedModelRevision = document.graph().findNode(edits.nodeId)->model->revision();
    for (int corner = 0; corner < 8; ++corner) {
        edits.edits.push_back({ TrimeshAuthoringEditKind::Vertex, corner, -1, "amp", 0.15f });
    }
    const auto result = commands.editTrimesh(edits);
    REQUIRE(result.succeeded());
    REQUIRE(result.changed);
    const auto changed = render();
    REQUIRE(bool(changed != baseline));
    REQUIRE(document.undo());
    CHECK(render() == baseline);
    REQUIRE(document.redo());
    CHECK(render() == changed);
    GraphDocument reopened;
    REQUIRE(reopened.loadJson(document.toJson(), false));
    CHECK(reopened.toJson() == document.toJson());
}
