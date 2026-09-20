#include <algorithm>

#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/NodeDefinition.h"
#include "UI/NodeCanvasQueryModel.h"

namespace CycleV2 {

namespace {

String nodeDisplayLabel(const Node& node) {
    return labelForNodeKind(node.kind);
}

String signalDescription(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:                 return "Audio";
        case PortDomain::SpectralMagnitudeSignal:    return "Spectrum levels";
        case PortDomain::SpectralPhaseSignal:        return "Spectrum timing";
        case PortDomain::ControlSignal:              return "A control value";
        case PortDomain::EnvelopeSignal:             return "An envelope";
        case PortDomain::PitchSignal:                return "Pitch";
        case PortDomain::VoiceControlSignal:         return "Voice settings";
        case PortDomain::DomainContext:              return "Voice setup";
        case PortDomain::MeshField:                  return "A shape";
    }

    return "A signal";
}

}

NodeCanvasQueryModel::NodeCanvasQueryModel(
        const NodeGraph& targetGraph,
        const GraphPresentationSnapshot& targetSnapshot)
    :   graph(targetGraph)
    ,   snapshot(&targetSnapshot) {
}

const Node* NodeCanvasQueryModel::findNode(const String& id) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const Node* NodeCanvasQueryModel::findNodeAt(Point<float> worldPosition) const {
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];

        if (node.bounds.contains(worldPosition)) {
            return &node;
        }
    }

    return nullptr;
}

const Port* NodeCanvasQueryModel::findPort(
        const Node& node,
        const String& portId,
        bool inputPort) const {
    const auto& ports = inputPort ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

const RuntimeNodeTrace* NodeCanvasQueryModel::findRuntimeTrace(const String& nodeId) const {
    return presentationFacts().runtimeTraceFor(*snapshot, nodeId);
}

const NodePreviewResult* NodeCanvasQueryModel::findPreviewResult(const String& nodeId) const {
    return presentationFacts().previewFor(*snapshot, nodeId);
}

PortDomain NodeCanvasQueryModel::displayDomainForEdge(const Edge& edge) const {
    return presentationFacts().domainForEdge(graph, edge);
}

PortDomain NodeCanvasQueryModel::displayDomainForNodeOutput(
        const Node& node,
        const String& portId) const {
    if (snapshot->compileResult.succeeded()) {
        for (const auto& step : snapshot->compileResult.plan.steps) {
            if (step.nodeId != node.id) {
                continue;
            }

            for (const auto& output : step.outputs) {
                if (output.portId == portId) {
                    return output.domain;
                }
            }
        }
    }

    for (const size_t edgeIndex : presentationFacts().edgeIndex().outgoingEdges(node.id)) {
        const Edge& edge = graph.getEdges()[edgeIndex];
        if (!edge.isAttachment() && edge.sourcePortId == portId) {
            return displayDomainForEdge(edge);
        }
    }

    if (const Port* port = findPort(node, portId, false)) {
        return port->domain;
    }

    return node.outputs.empty() ? PortDomain::ControlSignal : node.outputs.front().domain;
}

TrimeshRenderProfile NodeCanvasQueryModel::renderProfileForNodeOutput(
        const Node& node,
        const String& portId) const {
    NodeRenderSemantic semantic = presentationFacts().renderSemanticForNodeOutput(
            graph, node.id, portId);

    if (semantic.domain == PortDomain::ControlSignal) {
        semantic.domain = displayDomainForNodeOutput(node, portId);
    }

    return TrimeshRenderProfile::fromSemantic(semantic);
}

bool NodeCanvasQueryModel::edgeHasValidationIssue(const Edge& edge) const {
    return GraphValidator().edgeHasValidationIssue(graph, edge);
}

GraphValidationIssue NodeCanvasQueryModel::validationIssueForEdge(const Edge& edge) const {
    return GraphValidator().validationIssueForEdge(graph, edge);
}

int NodeCanvasQueryModel::executionIndexForNode(const String& nodeId) const {
    if (!snapshot->compileResult.succeeded()) {
        return -1;
    }
    return presentationFacts().executionIndexFor(nodeId);
}

int NodeCanvasQueryModel::attachmentCount() const {
    return presentationFacts().attachmentCount();
}

String NodeCanvasQueryModel::hoverTextForPort(const PortAddress& address) const {
    const Node* node = findNode(address.nodeId);

    if (node == nullptr) {
        return {};
    }

    const Port* port = findPort(*node, address.portId, address.input);

    if (port == nullptr) {
        return {};
    }

    if (port->purpose == PortPurpose::ScratchAttachment) {
        if (node->kind == NodeKind::VoiceContext) {
            return "Attach the default scratch envelope for this Voice Context.";
        }

        const auto& localEdges = presentationFacts().edgeIndex().edgesToInput(
                node->id, port->id);
        if (!localEdges.empty()) {
            const Edge& local = graph.getEdges()[localEdges.front()];
            const Node* source = findNode(local.sourceNodeId);
            return source != nullptr && source->kind == NodeKind::ScratchDefaultOverride
                    ? "Uses voice time instead of the inherited scratch envelope."
                    : "Overrides the Voice Context scratch envelope for this Trimesh.";
        }

        if (snapshot->compileResult.succeeded()) {
            const auto step = std::find_if(
                    snapshot->compileResult.plan.steps.begin(),
                    snapshot->compileResult.plan.steps.end(),
                    [&](const GraphExecutionStep& candidate) {
                        return candidate.nodeId == node->id;
                    });
            if (step != snapshot->compileResult.plan.steps.end()
                    && effectiveScratchSourceNodeId(*step).isNotEmpty()) {
                return "Inherits the Voice Context scratch envelope. Attach here to override it.";
            }
        }
        return "Uses voice time. Attach a scratch envelope here to override it.";
    }

    if (node->kind == NodeKind::ScratchDefaultOverride && !address.input) {
        return "Connect to a Trimesh scratch port to use voice time instead of the context default.";
    }

    return signalDescription(port->domain)
            + (address.input ? " enters " : " leaves ")
            + nodeDisplayLabel(*node) + " here.";
}

String NodeCanvasQueryModel::hoverTextForNode(const Node& node) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(node.kind);
    if (definition != nullptr && definition->helpText.isNotEmpty()) {
        return definition->helpText;
    }

    return nodeDisplayLabel(node) + ".";
}

String NodeCanvasQueryModel::hoverTextForEdge(const Edge& edge) const {
    const auto issue = validationIssueForEdge(edge);
    const Node* sourceNode = findNode(edge.sourceNodeId);
    const Node* destinationNode = findNode(edge.destNodeId);
    const String source = sourceNode != nullptr
            ? nodeDisplayLabel(*sourceNode)
            : edge.sourceNodeId;
    const String destination = destinationNode != nullptr
            ? nodeDisplayLabel(*destinationNode)
            : edge.destNodeId;
    const String route = source
            + " to "
            + destination;

    if (issue.message.isNotEmpty()) {
        return "This connection is invalid: " + issue.message + ". Route: " + route + ".";
    }

    if (edge.isAttachment()) {
        if (sourceNode != nullptr && sourceNode->kind == NodeKind::ScratchDefaultOverride) {
            return "Stops " + destination + " from inheriting the Voice Context scratch envelope.";
        }
        if (destinationNode != nullptr
                && destinationNode->kind == NodeKind::VoiceContext
                && edge.destPortId == "scratch") {
            return "Sets the default scratch envelope for " + destination + ".";
        }
        if (destinationNode != nullptr
                && destinationNode->kind == NodeKind::TrilinearMesh
                && edge.destPortId == "scratch") {
            return "Overrides the Voice Context scratch envelope for " + destination + ".";
        }
        return "Controls " + destination + " from " + source + ".";
    }

    return signalDescription(displayDomainForEdge(edge)) + " flows from " + route + ".";
}

const GraphPresentationFacts& NodeCanvasQueryModel::presentationFacts() const {
    if (snapshot->facts != nullptr) {
        return *snapshot->facts;
    }
    if (fallbackFacts == nullptr) {
        fallbackFacts = std::make_shared<const GraphPresentationFacts>(graph, *snapshot);
    }
    return *fallbackFacts;
}

}
