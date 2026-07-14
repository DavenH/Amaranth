#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphValidator.h"

#include <algorithm>

using namespace CycleV2;

TEST_CASE("Demo graph validates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    REQUIRE(GraphValidator().isValid(graph));
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
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });

    REQUIRE(GraphValidator().isValid(graph));
    REQUIRE(labelForDomain(PortDomain::ControlSignal) == "Universal");
}

TEST_CASE("Operation nodes reject mixed concrete signal domains", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "time",
            NodeKind::GenericProcessor,
            "Time",
            {},
            {},
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            "Magnitude",
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "time", "out", "add", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "mag", "out", "add", "right", PortDomain::SpectralMagnitudeSignal, false });

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
    graph.addEdge({ "fft", "phase", "multiply", "left", PortDomain::SpectralPhaseSignal, false });

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
    graph.addEdge({ "fft", "phase", "add", "left", PortDomain::SpectralPhaseSignal, false });

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Operation nodes reject mixed resolved source domains", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode({
            "time",
            NodeKind::GenericProcessor,
            "Time",
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "time", "out", "add", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, false });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MixedOperationDomains;
            }));
}

TEST_CASE("Context-resolved spectral sources cannot feed time-only transforms", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 460.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, false });

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

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 460.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::ControlSignal, false });

    const GraphValidator validator;
    const Edge& contextEdge = graph.getEdges()[0];
    const Edge& signalEdge = graph.getEdges()[1];

    REQUIRE_FALSE(validator.isValid(graph));
    REQUIRE_FALSE(validator.edgeHasValidationIssue(graph, contextEdge));
    REQUIRE(validator.edgeHasValidationIssue(graph, signalEdge));
    REQUIRE(validator.resolvedDomainForEdge(graph, signalEdge) == PortDomain::SpectralMagnitudeSignal);

    graph.replaceNodeParameters("voice", {
            { "domain", "Start Domain", "waveform" }
    });

    REQUIRE(validator.isValid(graph));
    REQUIRE_FALSE(validator.edgeHasValidationIssue(graph, signalEdge));
    REQUIRE(validator.resolvedDomainForEdge(graph, signalEdge) == PortDomain::TimeSignal);
}

TEST_CASE("Edge validation reports specific grammar diagnostics", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, false });

    const auto issue = GraphValidator().validationIssueForEdge(graph, graph.getEdges().front());

    REQUIRE(issue.code == GraphValidationCode::DomainMismatch);
    REQUIRE(issue.message.contains("Wave source requires waveform Voice Context"));
    REQUIRE(issue.sourceNodeId == "voice");
    REQUIRE(issue.sourcePortId == "context");
    REQUIRE(issue.destNodeId == "wave");
    REQUIRE(issue.destPortId == "context");
}

TEST_CASE("Context-resolved spectral sources can seed additive spectral graphs", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Uncontexted mesh operands inherit operation signal domains", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            "Magnitude",
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 220.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 460.f, 0.f }));
    graph.addEdge({ "mag", "out", "add", "left", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, false });

    const GraphValidator validator;

    REQUIRE(validator.isValid(graph));
    REQUIRE(validator.resolvedDomainForEdge(graph, graph.getEdges()[1]) == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Spectral voice context marks fixed wave sources invalid", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, false });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch;
            }));
}

TEST_CASE("Fixed wave source context validity follows voice domain parameter", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "waveform" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, false });

    REQUIRE(GraphValidator().isValid(graph));

    graph.replaceNodeParameters("voice", {
            { "domain", "Start Domain", "spectral" }
    });

    REQUIRE_FALSE(GraphValidator().isValid(graph));

    graph.replaceNodeParameters("voice", {
            { "domain", "Start Domain", "waveform" }
    });

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Domain context cannot connect to ordinary universal signal ports", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 220.f, 0.f }));
    graph.addEdge({ "voice", "context", "multiply", "left", PortDomain::DomainContext, false });

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
    graph.addEdge({ "env", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal, false });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ScratchPortRequiresAttachment;
            }));
}

TEST_CASE("Trimesh guide targets require guide curve attachment sources", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "env", "env", "waveMesh", "guide.vertex.0.amp", PortDomain::EnvelopeSignal, true });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::InvalidAttachmentSource;
            }));
}

TEST_CASE("Pitch cannot feed non voice-aware processors", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            "Pitch",
            {},
            {},
            {},
            {},
            { { "out", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Multiply, "multiply", {}));
    graph.addEdge({ "pitch", "out", "multiply", "left", PortDomain::PitchSignal, false });

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
            "Source",
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
            "Dest",
            {},
            {},
            {},
            {
                    { "time", "Time Mono", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, true }
            },
            {}
    });
    graph.addEdge({ "source", "time", "dest", "time", PortDomain::TimeSignal, false });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ChannelLayoutMismatch;
            }));
}
