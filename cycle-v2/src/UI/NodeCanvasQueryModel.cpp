#include "NodeCanvasQueryModel.h"

#include "../Graph/GraphRenderSemanticResolver.h"

namespace CycleV2 {

namespace {

String portDisplayLabel(const Port& port) {
    const String channel = labelForChannelLayout(port.channelLayout);
    return channel.isEmpty() ? port.label : port.label + " " + channel;
}

String nodeDisplayLabel(const Node& node) {
    return labelForNodeKind(node.kind);
}

String countPhrase(int count, const String& singular) {
    return String(count) + " " + singular + (count == 1 ? String {} : "s");
}

String endpointDescription(
        const Node* node,
        const Port* port,
        const String& fallbackNodeId,
        const String& fallbackPortId) {
    const String nodeLabel = node != nullptr ? nodeDisplayLabel(*node) : fallbackNodeId;
    const String portLabel = port != nullptr ? portDisplayLabel(*port) : fallbackPortId;
    return nodeLabel + " “" + portLabel + "”";
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

    String text = portDisplayLabel(*port) + " is the "
            + labelForDomain(port->domain).toLowerCase()
            + (address.input ? " input on " : " output from ")
            + nodeDisplayLabel(*node) + ".";

    if (port->purpose == PortPurpose::ScratchAttachment) {
        text += " It accepts a scratch-envelope attachment.";
    }

    if (port->channelLayout != ChannelLayout::Mono) {
        text += " It carries " + labelForChannelLayout(port->channelLayout) + ".";
    }

    return text;
}

String NodeCanvasQueryModel::hoverTextForNode(const Node& node) const {
    String text = nodeDisplayLabel(node);
    if (node.subtitle.isNotEmpty()) {
        text += " — " + node.subtitle + " —";
    }
    text += " has "
            + countPhrase((int) node.inputs.size(), "input") + " and "
            + countPhrase((int) node.outputs.size(), "output") + ".";
    const RuntimeNodeTrace* trace = findRuntimeTrace(node.id);

    if (trace != nullptr && !trace->signalOutputs.empty()) {
        text += " It produces ";

        for (size_t i = 0; i < trace->signalOutputs.size(); ++i) {
            if (i > 0) {
                text += ", ";
            }

            text += labelForDomain(trace->signalOutputs[i].domain)
                    + " from “" + trace->signalOutputs[i].portId + "”";
        }
        text += ".";
    }

    return text;
}

String NodeCanvasQueryModel::hoverTextForEdge(const Edge& edge) const {
    const auto issue = validationIssueForEdge(edge);
    const Node* sourceNode = findNode(edge.sourceNodeId);
    const Node* destinationNode = findNode(edge.destNodeId);
    const Port* sourcePort = sourceNode != nullptr
            ? findPort(*sourceNode, edge.sourcePortId, false)
            : nullptr;
    const Port* destinationPort = destinationNode != nullptr
            ? findPort(*destinationNode, edge.destPortId, true)
            : nullptr;
    const String route = endpointDescription(
            sourceNode,
            sourcePort,
            edge.sourceNodeId,
            edge.sourcePortId)
            + " to "
            + endpointDescription(
                    destinationNode,
                    destinationPort,
                    edge.destNodeId,
                    edge.destPortId);

    if (issue.message.isNotEmpty()) {
        return "This connection is invalid: " + issue.message + ". Route: " + route + ".";
    }

    return labelForDomain(displayDomainForEdge(edge))
            + (edge.isAttachment() ? " control attachment from " : " signal from ")
            + route + ".";
}

}
