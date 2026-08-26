#include "UI/NodeCanvasQueryModel.h"

#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/NodeDefinition.h"

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
        const GraphCompileResult& targetCompileResult,
        const RuntimeProcessTrace& targetRuntimeTrace,
        const GraphPreviewResult& targetPreviewResult)
    :   graph(targetGraph)
    ,   compileResult(targetCompileResult)
    ,   runtimeTrace(targetRuntimeTrace)
    ,   previewResult(targetPreviewResult) {
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
    for (const auto& node : runtimeTrace.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

const NodePreviewResult* NodeCanvasQueryModel::findPreviewResult(const String& nodeId) const {
    for (const auto& node : previewResult.nodes) {
        if (node.nodeId == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

PortDomain NodeCanvasQueryModel::displayDomainForEdge(const Edge& edge) const {
    if (edge.isAttachment()) {
        return edge.domain;
    }

    return GraphValidator().resolvedDomainForEdge(graph, edge);
}

PortDomain NodeCanvasQueryModel::displayDomainForNodeOutput(
        const Node& node,
        const String& portId) const {
    if (compileResult.succeeded()) {
        for (const auto& step : compileResult.plan.steps) {
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

    for (const auto& edge : graph.getEdges()) {
        if (!edge.isAttachment() && edge.sourceNodeId == node.id && edge.sourcePortId == portId) {
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
    NodeRenderSemantic semantic = GraphRenderSemanticResolver().semanticForNodeOutput(
            graph,
            node.id,
            portId);

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
    if (!compileResult.succeeded()) {
        return -1;
    }

    const auto& nodeOrder = compileResult.plan.nodeOrder;

    for (int i = 0; i < (int) nodeOrder.size(); ++i) {
        if (nodeOrder[(size_t) i] == nodeId) {
            return i;
        }
    }

    return -1;
}

int NodeCanvasQueryModel::attachmentCount() const {
    int count = 0;

    for (const auto& edge : graph.getEdges()) {
        if (edge.isAttachment()) {
            ++count;
        }
    }

    return count;
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
        return "Attach a scratch envelope here.";
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
        return "Controls " + destination + " from " + source + ".";
    }

    return signalDescription(displayDomainForEdge(edge)) + " flows from " + route + ".";
}

}
