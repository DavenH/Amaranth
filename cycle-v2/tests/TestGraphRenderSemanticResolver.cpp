#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphRenderSemanticResolver.h"

#include <utility>

using namespace CycleV2;

namespace {

Node spectralMagnitudeSource(String id) {
    return {
            std::move(id),
            NodeKind::GenericProcessor,
            "Magnitude",
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    };
}

Node pitchConsumer(String id) {
    return {
            std::move(id),
            NodeKind::GenericProcessor,
            "Pitch Consumer",
            {},
            {},
            {},
            { { "pitch", "Pitch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::Signal, true } },
            {}
    };
}

}

TEST_CASE("Render semantics resolve additive and multiplicative spectral scale", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph additiveGraph;
    NodeGraph multiplicativeGraph;

    additiveGraph.addNode(spectralMagnitudeSource("mag"));
    additiveGraph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    additiveGraph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    additiveGraph.addEdge({ "mag", "out", "add", "left", PortDomain::SpectralMagnitudeSignal, false });
    additiveGraph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, false });

    multiplicativeGraph.addNode(spectralMagnitudeSource("mag"));
    multiplicativeGraph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    multiplicativeGraph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 460.f, 0.f }));
    multiplicativeGraph.addEdge({ "mag", "out", "multiply", "left", PortDomain::SpectralMagnitudeSignal, false });
    multiplicativeGraph.addEdge({ "mesh", "out", "multiply", "right", PortDomain::ControlSignal, false });

    const NodeRenderSemantic additive = resolver.semanticForNodeOutput(additiveGraph, "mesh", "out");
    const NodeRenderSemantic multiplicative = resolver.semanticForNodeOutput(multiplicativeGraph, "mesh", "out");

    REQUIRE(additive.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(additive.scalePolicy == RenderScalePolicy::Unipolar);
    REQUIRE(additive.role == RenderSemanticRole::SpectralMagnitudeAdditive);
    REQUIRE(multiplicative.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(multiplicative.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(multiplicative.role == RenderSemanticRole::SpectralMagnitudeMultiplicative);
}

TEST_CASE("Render semantics resolve envelope scale from downstream target", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 220.f, 0.f }));
    graph.addNode(pitchConsumer("pitch"));
    graph.addEdge({ "env", "env", "pitch", "pitch", PortDomain::EnvelopeSignal, false });

    const NodeRenderSemantic semantic = resolver.semanticForNodeOutput(graph, "env", "env");

    REQUIRE(semantic.domain == PortDomain::EnvelopeSignal);
    REQUIRE(semantic.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(semantic.role == RenderSemanticRole::EnvelopeBipolar);
}
