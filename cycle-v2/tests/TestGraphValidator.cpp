#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphAudioScope.h"
#include "Graph/GraphValidationContext.h"
#include "Graph/GraphValidator.h"

using namespace CycleV2;

namespace {

bool addressesEdge(const GraphValidationIssue& issue, const Edge& edge) {
    return issue.sourceNodeId == edge.sourceNodeId
        && issue.sourcePortId == edge.sourcePortId
        && issue.destNodeId == edge.destNodeId
        && issue.destPortId == edge.destPortId;
}

void setParameter(Node& node, const String& id, const String& value) {
    const auto found = std::find_if(
            node.parameters.begin(),
            node.parameters.end(),
            [&](const NodeParameter& parameter) {
                return parameter.id == id;
            });
    REQUIRE(found != node.parameters.end());
    found->value = value;
}

void requireEdgeQueriesMatchBulkValidation(const NodeGraph& graph) {
    const GraphValidator validator;
    const auto issues = validator.validate(graph);

    for (const auto& edge : graph.getEdges()) {
        const auto first = std::find_if(
                issues.begin(),
                issues.end(),
                [&edge](const GraphValidationIssue& issue) {
                    return addressesEdge(issue, edge);
                });
        const auto queried = validator.validationIssueForEdge(graph, edge);

        REQUIRE(validator.edgeHasValidationIssue(graph, edge) == (first != issues.end()));

        if (first == issues.end()) {
            REQUIRE(queried.message.isEmpty());
            continue;
        }

        REQUIRE(queried.code == first->code);
        REQUIRE(queried.message == first->message);
        REQUIRE(addressesEdge(queried, edge));
    }
}

}

TEST_CASE("Demo graph validates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Proposed graph issues must strictly repair existing issues",
        "[cycle-v2][graph][validation]") {
    const GraphValidationIssue first {
            GraphValidationCode::DomainMismatch, "First issue", "wave", "out", "effect", "in"
    };
    const GraphValidationIssue second {
            GraphValidationCode::ProcessingScopeMismatch,
            "Second issue", "effect", "out", "output", "time"
    };
    GraphValidationIssue changedAddress = first;
    changedAddress.destPortId = "other";
    GraphValidationIssue changedSubject = first;
    changedSubject.subjectId = "other";

    REQUIRE(GraphValidator::acceptsProposedIssues({ first, second }, {}));
    REQUIRE(GraphValidator::acceptsProposedIssues({ first, second }, { first }));
    REQUIRE_FALSE(GraphValidator::acceptsProposedIssues({}, { first }));
    REQUIRE_FALSE(GraphValidator::acceptsProposedIssues({ first }, { first }));
    REQUIRE_FALSE(GraphValidator::acceptsProposedIssues(
            { first, second }, { changedAddress }));
    REQUIRE_FALSE(GraphValidator::acceptsProposedIssues(
            { first, second }, { changedSubject }));
}

TEST_CASE("Boundary-free graph fragments retain legacy validation semantics",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "wave", "out", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    const auto issues = GraphValidator().validate(graph);
    REQUIRE(std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.code == GraphValidationCode::MissingRequiredNode;
    }));
}

TEST_CASE("Demo graph exposes stable node kinds", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto& nodes = graph.getNodes();

    const auto findKind = [&](const String& nodeId) {
        const auto found = std::find_if(
                nodes.begin(),
                nodes.end(),
                [&](const Node& node) {
                    return node.id == nodeId;
                });
        REQUIRE(found != nodes.end());
        return found->kind;
    };

    REQUIRE(findKind("voice") == NodeKind::VoiceContext);
    REQUIRE(findKind("waveMesh") == NodeKind::TrilinearMesh);
    REQUIRE(findKind("fft") == NodeKind::Fft);
    REQUIRE(findKind("addMag") == NodeKind::Add);
    REQUIRE(findKind("addPhase") == NodeKind::Add);
    REQUIRE(findKind("ifft") == NodeKind::Ifft);
    REQUIRE(findKind("multiply") == NodeKind::Multiply);
    REQUIRE(findKind("out") == NodeKind::Output);
}

TEST_CASE("Demo graph uses one canonical trilinear mesh port schema", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    std::vector<const Node*> meshes;

    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::TrilinearMesh) {
            meshes.push_back(&node);
        }
    }

    REQUIRE(meshes.size() == 3);

    for (const Node* mesh : meshes) {
        REQUIRE(mesh->inputs.size() == meshes.front()->inputs.size());
        REQUIRE(mesh->outputs.size() == meshes.front()->outputs.size());

        for (size_t i = 0; i < mesh->inputs.size(); ++i) {
            REQUIRE(mesh->inputs[i].id == meshes.front()->inputs[i].id);
            REQUIRE(mesh->inputs[i].domain == meshes.front()->inputs[i].domain);
            REQUIRE(mesh->inputs[i].purpose == meshes.front()->inputs[i].purpose);
        }

        for (size_t i = 0; i < mesh->outputs.size(); ++i) {
            REQUIRE(mesh->outputs[i].id == meshes.front()->outputs[i].id);
            REQUIRE(mesh->outputs[i].domain == meshes.front()->outputs[i].domain);
            REQUIRE(mesh->outputs[i].purpose == meshes.front()->outputs[i].purpose);
        }
    }
}

TEST_CASE("Channel layouts have stable short labels", "[cycle-v2][graph]") {
    REQUIRE(labelForChannelLayout(ChannelLayout::Mono).isEmpty());
    REQUIRE(labelForChannelLayout(ChannelLayout::LinkedStereo) == "L/R");
    REQUIRE(labelForChannelLayout(ChannelLayout::Left) == "L");
    REQUIRE(labelForChannelLayout(ChannelLayout::Right) == "R");
    REQUIRE(labelForChannelLayout(ChannelLayout::StereoPair) == "Pair");
}

TEST_CASE("Universal ports accept typed graph operands", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, ConnectionKind::Signal });

    REQUIRE(GraphValidator().isValid(graph));
    REQUIRE(labelForDomain(PortDomain::ControlSignal) == "Universal");
}

TEST_CASE("Pan accepts time signals and rejects non-audio control signals",
        "[cycle-v2][graph][pan]") {
    GraphNodeFactory factory;
    NodeGraph timeGraph;
    timeGraph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    timeGraph.addNode(factory.createNode(NodeKind::SpectralLayer, "pan", {}));
    timeGraph.addEdge({
            "wave", "out", "pan", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphValidator().isValid(timeGraph));

    NodeGraph controlGraph;
    controlGraph.addNode(factory.createNode(NodeKind::ModulationSource, "mod", {}));
    controlGraph.addNode(factory.createNode(NodeKind::SpectralLayer, "pan", {}));
    controlGraph.addEdge({
            "mod", "value", "pan", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    const auto issues = GraphValidator().validate(controlGraph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch;
            }));
}

TEST_CASE("Operation nodes reject mixed concrete signal domains", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "time",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "time", "out", "add", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "mag", "out", "add", "right", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MixedOperationDomains;
            }));
}

TEST_CASE("Multiply rejects spectral phase operations", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 320.f, 0.f }));
    graph.addEdge({ "fft", "phase", "multiply", "left", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch;
            }));
}

TEST_CASE("Add accepts spectral phase operations", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "fft", "phase", "add", "left", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Operation nodes reject mixed resolved source domains", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");

    graph.addNode({
            "time",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "time", "out", "add", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MixedOperationDomains;
            }));
}

TEST_CASE("Explicit spectral sources cannot feed time-only transforms", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");

    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 460.f, 0.f }));
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch;
            }));
}

TEST_CASE("Resolved edge domains update while graph is invalid", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");

    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 460.f, 0.f }));
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, ConnectionKind::Signal });

    const GraphValidator validator;
    const Edge& signalEdge = graph.getEdges()[0];

    REQUIRE_FALSE(validator.isValid(graph));
    REQUIRE(validator.edgeHasValidationIssue(graph, signalEdge));
    REQUIRE(validator.resolvedDomainForEdge(graph, signalEdge) == PortDomain::SpectralMagnitudeSignal);

    setParameter(*graph.findNodeForEditing("mesh"), "signalType", "time");

    REQUIRE(validator.isValid(graph));
    REQUIRE_FALSE(validator.edgeHasValidationIssue(graph, signalEdge));
    REQUIRE(validator.resolvedDomainForEdge(graph, signalEdge) == PortDomain::TimeSignal);
}

TEST_CASE("Edge validation reports specific grammar diagnostics", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");

    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 460.f, 0.f }));
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, ConnectionKind::Signal });

    const auto issue = GraphValidator().validationIssueForEdge(graph, graph.getEdges().front());

    REQUIRE(issue.code == GraphValidationCode::DomainMismatch);
    REQUIRE(issue.message.contains("Mag -> Time"));
    REQUIRE(issue.sourceNodeId == "mesh");
    REQUIRE(issue.sourcePortId == "out");
    REQUIRE(issue.destNodeId == "fft");
    REQUIRE(issue.destPortId == "time");
}

TEST_CASE("Explicit spectral sources can seed additive spectral graphs", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");

    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, ConnectionKind::Signal });

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Trimesh operands retain their explicit signal domains", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f });
    setParameter(mesh, "signalType", "spectralMagnitude");
    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "mag", "out", "add", "left", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, ConnectionKind::Signal });

    const GraphValidator validator;

    REQUIRE(validator.isValid(graph));
    REQUIRE(validator.resolvedDomainForEdge(graph, graph.getEdges()[1]) == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Operation domain inference excludes Envelope and Mesh products",
        "[cycle-v2][graph][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "envelope", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph,
            "envelope",
            "purpose",
            "Purpose",
            "scratch").succeeded());
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addEdge({ "envelope", "env", "add", "left", PortDomain::EnvelopeSignal, ConnectionKind::Signal });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, ConnectionKind::Signal });

    const auto resolution = GraphDomainResolver().resolve(graph);

    REQUIRE(resolution.domains[0] == PortDomain::EnvelopeSignal);
    REQUIRE(resolution.domains[1] == PortDomain::TimeSignal);
}

TEST_CASE("Explicit Trimesh domain propagates through a spectral layer",
        "[cycle-v2][graph][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    setParameter(mesh, "signalType", "spectralMagnitude");
    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "layer", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({
            "mesh", "out", "layer", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "layer", "out", "multiply", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "multiply", "out", "ifft", "mag",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    const auto resolution = GraphDomainResolver().resolve(graph);

    REQUIRE(resolution.domains[0] == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(resolution.domains[1] == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(resolution.domains[2] == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Domain resolution terminates deterministically for invalid cycles",
        "[cycle-v2][graph][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Add, "first", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "second", {}));
    graph.addEdge({ "first", "out", "second", "left", PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "second", "out", "first", "left", PortDomain::ControlSignal, ConnectionKind::Signal });

    const GraphDomainResolver resolver;
    const auto first = resolver.resolve(graph);
    const auto second = resolver.resolve(graph);

    REQUIRE(first.domains == second.domains);
    REQUIRE(first.channelLayouts == second.channelLayouts);
    REQUIRE(first.domains == std::vector<PortDomain> {
            PortDomain::ControlSignal,
            PortDomain::ControlSignal
    });
}

TEST_CASE("Proposed edge replacement resolves propagated domains without mutating the graph",
        "[cycle-v2][graph][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    setParameter(mesh, "signalType", "spectralMagnitude");
    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "layer", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addEdge({
            "mesh", "out", "layer", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "layer", "out", "multiply", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    const Edge replacement {
            "wave", "out", "layer", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    };
    NodeGraph committed = graph;
    committed.removeEdgeAt(0);
    committed.addEdge(replacement);

    const GraphDomainResolver resolver;
    const GraphEdgeView proposed(graph.getEdges(), { 0 }, { replacement });
    const GraphEdgeIndex baseIndex(graph.getEdges());
    const GraphEdgeIndexOverlay proposedIndex(baseIndex, proposed);
    const auto baselineResolution = resolver.resolve(graph);
    const auto resolved = resolver.resolve(
            graph,
            proposed,
            proposedIndex,
            baselineResolution);
    const auto committedResolution = resolver.resolve(committed);

    REQUIRE(resolved.domains == committedResolution.domains);
    REQUIRE(resolved.channelLayouts == committedResolution.channelLayouts);
    REQUIRE(resolved.domains[0] == PortDomain::TimeSignal);
    REQUIRE(resolved.domains[1] == PortDomain::TimeSignal);
    REQUIRE(graph.getEdges()[0].sourceNodeId == "mesh");
}

TEST_CASE("Proposed edge removal clears propagated domains from the affected branch",
        "[cycle-v2][graph][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    setParameter(mesh, "signalType", "spectralMagnitude");
    graph.addNode(std::move(mesh));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "layer", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "final", {}));
    graph.addEdge({
            "mesh", "out", "layer", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "layer", "out", "multiply", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "multiply", "out", "final", "left",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });

    const GraphDomainResolver resolver;
    const auto baseline = resolver.resolve(graph);
    const GraphEdgeIndex baseIndex(graph.getEdges());
    const GraphEdgeView proposed(graph.getEdges(), { 0 }, {});
    const GraphEdgeIndexOverlay proposedIndex(baseIndex, proposed);
    const auto resolved = resolver.resolve(graph, proposed, proposedIndex, baseline);
    const auto expected = resolver.resolve(graph, proposed);

    REQUIRE(resolved.domains == expected.domains);
    REQUIRE(resolved.channelLayouts == expected.channelLayouts);
    REQUIRE(resolved.domains == std::vector<PortDomain> {
            PortDomain::ControlSignal,
            PortDomain::ControlSignal
    });
    REQUIRE(graph.getEdges().size() == 3);
}

TEST_CASE("Voice Context carries oscillator configuration without a signal domain", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, ConnectionKind::Signal });

    REQUIRE(GraphValidator().isValid(graph));

    REQUIRE(parameterValueForNode(*graph.findNode("voice"), "domain").isEmpty());
}

TEST_CASE("Domain context cannot connect to ordinary universal signal ports", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "multiply", "left", PortDomain::DomainContext, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch;
            }));
}

TEST_CASE("Scratch ports require attachment routing", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "env", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal, ConnectionKind::Signal });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ScratchPortRequiresAttachment;
            }));
}

TEST_CASE("Voice-time scratch overrides attach only to Trimesh scratch ports",
        "[cycle-v2][graph][voice-context][scratch]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ScratchDefaultOverride, "voiceTime", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));

    GraphEditor editor;
    REQUIRE(editor.connect(
            graph,
            { "voiceTime", "scratch", false },
            { "mesh", "scratch", true }).succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    const GraphEditResult invalid = editor.connect(
            graph,
            { "voiceTime", "scratch", false },
            { "voice", "scratch", true });
    REQUIRE_FALSE(invalid.succeeded());
    REQUIRE(std::any_of(
            invalid.validationIssues.begin(),
            invalid.validationIssues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::InvalidAttachmentDestination;
            }));
}

TEST_CASE("Synthetic Trimesh guide ports are rejected", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "env", "env", "waveMesh", "guide.cube.0.amp", PortDomain::EnvelopeSignal, ConnectionKind::ProcessingAttachment });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MissingDestinationPort;
            }));
}

TEST_CASE("Pitch cannot feed non voice-aware processors", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "out", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Multiply, "multiply", {}));
    graph.addEdge({ "pitch", "out", "multiply", "left", PortDomain::PitchSignal, ConnectionKind::Signal });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch
                    || issue.code == GraphValidationCode::PitchRequiresVoiceAwareDestination;
            }));
}

TEST_CASE("Audio signal edges require compatible channel layouts", "[cycle-v2][graph]") {
    NodeGraph graph;

    graph.addNode({
            "source",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            {
                    { "time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false }
            }
    });
    graph.addNode({
            "dest",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {
                    { "time", "Time Mono", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, true }
            },
            {}
    });
    graph.addEdge({ "source", "time", "dest", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ChannelLayoutMismatch;
            }));
}

TEST_CASE("Edge queries use the authoritative bulk validation rules", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode({
            "source",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "pitch", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "guide",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "curve", "Curve", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "dest",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {
                    { "time", "Time", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, true },
                    { "attachmentTarget", "Attachment Target", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, true },
                    { "scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment, true }
            },
            {}
    });
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));

    graph.addEdge({ "missingSource", "out", "dest", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "source", "time", "missingDest", "in", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "source", "missing", "dest", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "source", "time", "dest", "missing", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "source", "time", "dest", "attachmentTarget", PortDomain::TimeSignal, ConnectionKind::ProcessingAttachment });
    graph.addEdge({ "source", "time", "dest", "scratch", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "guide", "curve", "mesh", "guide.cube.0.amp", PortDomain::TimeSignal, ConnectionKind::ProcessingAttachment });
    graph.addEdge({ "source", "time", "mesh", "guide.cube.1.amp", PortDomain::TimeSignal, ConnectionKind::ProcessingAttachment });
    graph.addEdge({ "source", "time", "dest", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "pitch", "pitch", "dest", "time", PortDomain::PitchSignal, ConnectionKind::Signal });
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, ConnectionKind::Signal });

    requireEdgeQueriesMatchBulkValidation(graph);

    const auto issues = GraphValidator().validate(graph);
    const Edge& invalidAttachment = graph.getEdges()[4];
    const auto issueCount = std::count_if(
            issues.begin(),
            issues.end(),
            [&invalidAttachment](const GraphValidationIssue& issue) {
                return addressesEdge(issue, invalidAttachment);
            });

    REQUIRE(issueCount == 2);
}

TEST_CASE("Explicit global audio graph accepts zero or one voice terminal",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "globalIn", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    REQUIRE(GraphValidator().isValid(graph));

    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "voiceTerminal", {}));
    graph.addEdge({ "voiceTerminal", "out", "voiceOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    REQUIRE(GraphValidator().isValid(graph));

    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "otherTerminal", {}));
    const auto issues = GraphValidator().validate(graph);
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::AmbiguousVoiceOutput;
            }));
}

TEST_CASE("Explicit global audio graph rejects cross-scope signal edges",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shaper", {}));
    graph.addEdge({
            "globalIn", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({ "wave", "out", "voiceOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    const Edge voiceToGlobal {
            "wave", "out", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    };
    const Edge globalToVoice {
            "globalIn", "time", "shaper", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    };
    graph.addEdge(voiceToGlobal);
    graph.addEdge(globalToVoice);

    const auto issues = GraphValidator().validate(graph);
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [&](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ProcessingScopeMismatch
                        && addressesEdge(issue, voiceToGlobal);
            }));
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [&](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ProcessingScopeMismatch
                        && addressesEdge(issue, globalToVoice);
            }));
}

TEST_CASE("Proposed edge validation matches the committed graph without copying it",
        "[cycle-v2][graph][audio-scope][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addEdge({
            "globalIn", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "wave", "out", "voiceOut", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    const Edge crossScope {
            "wave", "out", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    };
    NodeGraph committed = graph;
    committed.addEdge(crossScope);

    InteractionComplexityDiagnostics::reset();
    const GraphEdgeView proposed(graph.getEdges(), {}, { crossScope });
    const GraphValidator validator;
    const auto proposedIssues = validator.validate(graph, proposed);
    const auto committedIssues = validator.validate(committed);

    REQUIRE(proposedIssues.size() == committedIssues.size());
    for (size_t index = 0; index < proposedIssues.size(); ++index) {
        REQUIRE(proposedIssues[index].code == committedIssues[index].code);
        REQUIRE(proposedIssues[index].message == committedIssues[index].message);
        REQUIRE(proposedIssues[index].sourceNodeId == committedIssues[index].sourceNodeId);
        REQUIRE(proposedIssues[index].destNodeId == committedIssues[index].destNodeId);
    }
    REQUIRE(std::any_of(proposedIssues.begin(), proposedIssues.end(), [](const auto& issue) {
        return issue.code == GraphValidationCode::ProcessingScopeMismatch;
    }));
    REQUIRE(graph.getEdges().size() == 2);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
}

TEST_CASE("Validation context retains one exact durable graph baseline",
        "[cycle-v2][graph][validation-context]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto expectedIssues = GraphValidator().validate(graph);
    const auto expectedDomains = GraphDomainResolver().resolve(graph);
    const GraphValidationContext context(graph);

    REQUIRE(context.matches(graph));
    REQUIRE(context.validationIssues().size() == expectedIssues.size());
    REQUIRE(context.domainResolution().domains == expectedDomains.domains);
    REQUIRE(context.domainResolution().channelLayouts
            == expectedDomains.channelLayouts);
    REQUIRE(context.edgeView().size() == graph.getEdges().size());

    InteractionComplexityDiagnostics::reset();
    REQUIRE(context.edgeIndex().incomingEdges("out").size() == 1);
    REQUIRE(context.audioScopeAnalysis().scopeFor("out")
            == AuthoredAudioScope::Global);
    const auto counts = InteractionComplexityDiagnostics::counts();
    REQUIRE(counts.validationNodeVisits == 0);
    REQUIRE(counts.validationEdgeVisits == 0);
    REQUIRE(counts.domainTransfers == 0);

    graph.markChanged();
    REQUIRE_FALSE(context.matches(graph));
}

TEST_CASE("Validation context recomputes affected operation policy",
        "[cycle-v2][graph][validation-context][domains]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addEdge({
            "wave", "out", "add", "left",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "fft", "mag", "add", "right",
            PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal
    });
    const GraphValidationContext context(graph);
    REQUIRE(std::any_of(
            context.validationIssues().begin(),
            context.validationIssues().end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MixedOperationDomains
                        && issue.subjectId == "add";
            }));

    const Edge replacement {
            "wave", "out", "add", "right",
            PortDomain::TimeSignal, ConnectionKind::Signal
    };
    const auto proposedIssues = context.validateProposal(graph, { 1 }, { replacement });
    const GraphEdgeView proposedEdges(graph.getEdges(), { 1 }, { replacement });
    const auto fullIssues = GraphValidator().validate(graph, proposedEdges);

    REQUIRE(proposedIssues.empty());
    REQUIRE(proposedIssues.size() == fullIssues.size());
}

TEST_CASE("Validation context updates Voice Context assignment policy",
        "[cycle-v2][graph][validation-context][voice-context]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "firstVoice", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "secondVoice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    const GraphValidationContext context(graph);
    REQUIRE(std::any_of(
            context.validationIssues().begin(),
            context.validationIssues().end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MissingVoiceContextAssignment
                        && issue.subjectId == "mesh";
            }));

    const Edge assignment {
            "firstVoice", "context", "mesh", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    };
    const auto proposedIssues = context.validateProposal(graph, {}, { assignment });
    const GraphEdgeView proposedEdges(graph.getEdges(), {}, { assignment });
    const auto fullIssues = GraphValidator().validate(graph, proposedEdges);

    REQUIRE(proposedIssues.empty());
    REQUIRE(proposedIssues.size() == fullIssues.size());
}

TEST_CASE("Neutral routing cannot participate in both audio partitions",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "route", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "globalIn", "time", "route", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "wave", "out", "route", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "route", "out", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    const auto issues = GraphValidator().validate(graph);
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ConflictingProcessingScope
                        && issue.message.contains("route");
            }));
}

TEST_CASE("Proposed edge removal updates neutral processing scope without mutating the graph",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "route", {}));
    graph.addEdge({
            "globalIn", "time", "route", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "wave", "out", "route", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    NodeGraph committed = graph;
    committed.removeEdgeAt(1);
    const GraphAudioScopeAnalyzer analyzer;
    const auto baseline = analyzer.analyze(graph);
    const GraphEdgeIndex baseIndex(graph.getEdges());
    const GraphEdgeView proposed(graph.getEdges(), { 1 }, {});
    const GraphEdgeIndexOverlay proposedIndex(baseIndex, proposed);
    const auto resolved = analyzer.analyze(
            graph,
            proposed,
            proposedIndex,
            baseline);
    const auto committedAnalysis = analyzer.analyze(committed);

    REQUIRE(resolved.nodes == committedAnalysis.nodes);
    REQUIRE(resolved.conflictingNeutralNodeIds == committedAnalysis.conflictingNeutralNodeIds);
    REQUIRE(resolved.scopeFor("route") == AuthoredAudioScope::Global);
    REQUIRE_FALSE(resolved.hasConflict("route"));
    REQUIRE(analyzer.analyze(graph).hasConflict("route"));
}

TEST_CASE("Every explicit global node belongs to the Global Input to Output path",
        "[cycle-v2][graph][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "globalIn", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });

    const auto issues = GraphValidator().validate(graph);
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::GlobalNodeUnreachable
                        && issue.message.contains("delay");
            }));
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::GlobalNodeCannotReachOutput
                        && issue.message.contains("delay");
            }));
}
