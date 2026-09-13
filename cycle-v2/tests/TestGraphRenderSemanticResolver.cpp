#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphRenderSemanticResolver.h"

#include <utility>

using namespace CycleV2;

namespace {

Node spectralMagnitudeSource(String id) {
    return {
            std::move(id),
            NodeKind::GenericProcessor,
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
            {},
            {},
            {},
            { { "pitch", "Pitch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::Signal, true } },
            {}
    };
}

}

TEST_CASE(
        "Trimesh render semantics keep polarity independent from graph arithmetic",
        "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph unipolarMultiplyGraph;
    NodeGraph bipolarAddGraph;

    Node unipolarMesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    unipolarMesh.parameters = {
            { "signalType", "Signal Type", "spectralMagnitude" },
            { "polarity", "Polarity", "unipolar" }
    };
    unipolarMultiplyGraph.addNode(spectralMagnitudeSource("mag"));
    unipolarMultiplyGraph.addNode(std::move(unipolarMesh));
    unipolarMultiplyGraph.addNode(
            factory.createNode(NodeKind::Multiply, "multiply", { 460.f, 0.f }));
    unipolarMultiplyGraph.addEdge({
            "mag", "out", "multiply", "left",
            PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal
    });
    unipolarMultiplyGraph.addEdge({
            "mesh", "out", "multiply", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    Node bipolarMesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    bipolarMesh.parameters = {
            { "signalType", "Signal Type", "spectralMagnitude" },
            { "polarity", "Polarity", "bipolar" }
    };
    bipolarAddGraph.addNode(spectralMagnitudeSource("mag"));
    bipolarAddGraph.addNode(std::move(bipolarMesh));
    bipolarAddGraph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    bipolarAddGraph.addEdge({
            "mag", "out", "add", "left",
            PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal
    });
    bipolarAddGraph.addEdge({
            "mesh", "out", "add", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    const NodeRenderSemantic unipolar = resolver.semanticForNodeOutput(
            unipolarMultiplyGraph, "mesh", "out");
    const NodeRenderSemantic bipolar = resolver.semanticForNodeOutput(
            bipolarAddGraph, "mesh", "out");

    REQUIRE(unipolar.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(unipolar.scalePolicy == RenderScalePolicy::Unipolar);
    REQUIRE(unipolar.role == RenderSemanticRole::SpectralMagnitudeUnipolar);
    REQUIRE(bipolar.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(bipolar.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(bipolar.role == RenderSemanticRole::SpectralMagnitudeBipolar);
}

TEST_CASE("Render semantics resolve envelope scale from downstream target", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 220.f, 0.f }));
    graph.addNode(pitchConsumer("pitch"));
    graph.addEdge({ "env", "env", "pitch", "pitch", PortDomain::EnvelopeSignal, ConnectionKind::Signal });

    const NodeRenderSemantic semantic = resolver.semanticForNodeOutput(graph, "env", "env");

    REQUIRE(semantic.domain == PortDomain::EnvelopeSignal);
    REQUIRE(semantic.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(semantic.role == RenderSemanticRole::EnvelopeBipolar);
}

TEST_CASE("Trimesh polarity exposes the source mesh magnitude semantic", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph graph;

    Node layer = factory.createNode(NodeKind::SpectralLayer, "layer", {});
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    mesh.parameters = {
            { "signalType", "Signal Type", "spectralMagnitude" },
            { "polarity", "Polarity", "bipolar" }
    };

    graph.addNode(std::move(mesh));
    graph.addNode(std::move(layer));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({ "mesh", "out", "layer", "in", PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "layer", "out", "ifft", "mag", PortDomain::ControlSignal, ConnectionKind::Signal });

    const NodeRenderSemantic semantic = resolver.semanticForNodeOutput(graph, "mesh", "out");

    REQUIRE(semantic.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(semantic.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(semantic.role == RenderSemanticRole::SpectralMagnitudeBipolar);
}

TEST_CASE(
        "Render semantics use explicit Trimesh type before downstream consumers exist",
        "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    GraphRenderSemanticResolver resolver;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    mesh.parameters = {
            { "signalType", "Signal Type", "spectralMagnitude" },
            { "polarity", "Polarity", "unipolar" }
    };

    graph.addNode(std::move(mesh));

    const NodeRenderSemantic semantic = resolver.semanticForNodeOutput(graph, "mesh", "out");

    REQUIRE(semantic.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(semantic.scalePolicy == RenderScalePolicy::Unipolar);
    REQUIRE(semantic.role == RenderSemanticRole::SpectralMagnitudeUnipolar);
}
