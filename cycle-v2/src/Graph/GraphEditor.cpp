#include "Graph/GraphEditor.h"

#include "Graph/GraphEdgeView.h"

namespace CycleV2 {

namespace {

PortDomain edgeDomainForConnection(const Port& source, const Port& dest) {
    if (source.domain == PortDomain::ControlSignal && dest.domain != PortDomain::ControlSignal) {
        return dest.domain;
    }

    return source.domain;
}

bool isProbeDomain(PortDomain domain) {
    return domain == PortDomain::TimeSignal
            || domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
}

bool sameValidationIssue(
        const GraphValidationIssue& first,
        const GraphValidationIssue& second) {
    return first.code == second.code
            && first.sourceNodeId == second.sourceNodeId
            && first.sourcePortId == second.sourcePortId
            && first.destNodeId == second.destNodeId
            && first.destPortId == second.destPortId;
}

bool strictlyRepairsValidationIssues(
        const std::vector<GraphValidationIssue>& before,
        const std::vector<GraphValidationIssue>& after) {
    if (before.empty() || after.size() >= before.size()) {
        return false;
    }
    return std::all_of(after.begin(), after.end(), [&](const auto& issue) {
        return std::any_of(before.begin(), before.end(), [&](const auto& prior) {
            return sameValidationIssue(issue, prior);
        });
    });
}

}

GraphEditResult GraphEditor::addNode(NodeGraph& graph, NodeKind kind, Point<float> position) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    if (definition != nullptr && definition->requiredSingleton) {
        const auto duplicate = std::find_if(
                graph.getNodes().begin(),
                graph.getNodes().end(),
                [kind](const Node& node) { return node.kind == kind; });
        if (duplicate != graph.getNodes().end()) {
            return { GraphEditCode::ValidationRejected, duplicate->id, {} };
        }
    }

    const String nodeId = createUniqueNodeId(graph, kind);
    graph.addNode(GraphNodeFactory().createNode(kind, nodeId, position));
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.topologyChanged = true;
    result.changes.layoutChanged = true;
    return result;
}

GraphEditResult GraphEditor::connect(
        NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) const {
    if (first.input == second.input) {
        return { GraphEditCode::DirectionMismatch, {}, {} };
    }

    const PortAddress& sourceAddress = first.input ? second : first;
    const PortAddress& destAddress = first.input ? first : second;

    const Node* sourceNode = findNode(graph, sourceAddress.nodeId);
    const Node* destNode = findNode(graph, destAddress.nodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    const Port* source = findPort(*sourceNode, sourceAddress.portId, false);
    const Port* dest = findPort(*destNode, destAddress.portId, true);

    if (source == nullptr || dest == nullptr) {
        return { GraphEditCode::MissingPort, {}, {} };
    }

    Edge proposedEdge {
            sourceAddress.nodeId,
            sourceAddress.portId,
            destAddress.nodeId,
            destAddress.portId,
            edgeDomainForConnection(*source, *dest),
            dest->purpose == PortPurpose::ScratchAttachment
                    ? ConnectionKind::ProcessingAttachment
                    : dest->connectionKind,
            dest->purpose == PortPurpose::ScratchAttachment
                    ? AttachmentType::ScratchEnvelope
                    : dest->attachmentType
    };

    std::vector<size_t> replacedEdgeIndices;
    for (size_t index = 0; index < graph.getEdges().size(); ++index) {
        const Edge& edge = graph.getEdges()[index];
        if (edge.destNodeId == destAddress.nodeId
                && edge.destPortId == destAddress.portId) {
            replacedEdgeIndices.push_back(index);
        }
    }
    const GraphEdgeView proposedEdges(
            graph.getEdges(),
            std::move(replacedEdgeIndices),
            { proposedEdge });

    GraphValidator validator;
    auto issues = validator.validate(graph, proposedEdges);

    if (!issues.empty()
            && !strictlyRepairsValidationIssues(validator.validate(graph), issues)) {
        return { GraphEditCode::ValidationRejected, {}, std::move(issues) };
    }

    graph.removeEdgesToInput(destAddress.nodeId, destAddress.portId);
    graph.addEdge(std::move(proposedEdge));
    GraphEditResult result;
    result.changes.nodeIds = { sourceAddress.nodeId, destAddress.nodeId };
    result.changes.topologyChanged = true;
    return result;
}

GraphEditResult GraphEditor::toggleSignalProbe(
        NodeGraph& graph,
        size_t edgeIndex,
        float tapPosition) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    const Edge& edge = graph.getEdges()[edgeIndex];
    if (edge.isAttachment()
            || !isProbeDomain(GraphValidator().resolvedDomainForEdge(graph, edge))) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    if (const auto* existing = graph.findSignalProbeForSource(
                edge.sourceNodeId, edge.sourcePortId)) {
        const String probeId = existing->id;
        graph.removeSignalProbe(probeId);
        GraphEditResult result { GraphEditCode::Connected, probeId, {} };
        result.changes.probesChanged = true;
        return result;
    }

    const String probeId = createUniqueProbeId(graph);
    int nextRailOrder {};
    for (const auto& probe : graph.getSignalProbes()) {
        nextRailOrder = jmax(nextRailOrder, probe.railOrder + 1);
    }
    graph.addSignalProbe({
            probeId,
            edge.sourceNodeId,
            edge.sourcePortId,
            edge.destNodeId,
            edge.destPortId,
            "Spy " + String(nextRailOrder + 1),
            jlimit(0.f, 1.f, tapPosition),
            nextRailOrder
    });
    GraphEditResult result { GraphEditCode::Connected, probeId, {} };
    result.changes.probesChanged = true;
    return result;
}

GraphEditResult GraphEditor::removeSignalProbe(NodeGraph& graph, const String& probeId) const {
    if (!graph.removeSignalProbe(probeId)) {
        return { GraphEditCode::MissingNode, probeId, {} };
    }
    GraphEditResult result { GraphEditCode::Connected, probeId, {} };
    result.changes.probesChanged = true;
    return result;
}

GraphEditResult GraphEditor::reattachSignalProbe(
        NodeGraph& graph,
        const String& probeId,
        size_t edgeIndex,
        float tapPosition) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, probeId, {} };
    }
    SignalProbe* probe = graph.findSignalProbeForEditing(probeId);
    if (probe == nullptr) {
        return { GraphEditCode::MissingNode, probeId, {} };
    }

    const Edge& edge = graph.getEdges()[edgeIndex];
    if (edge.isAttachment()
            || !isProbeDomain(GraphValidator().resolvedDomainForEdge(graph, edge))) {
        return { GraphEditCode::ValidationRejected, probeId, {} };
    }
    const SignalProbe* existing = graph.findSignalProbeForSource(
            edge.sourceNodeId, edge.sourcePortId);
    if (existing != nullptr && existing->id != probeId) {
        return { GraphEditCode::ValidationRejected, probeId, {} };
    }

    probe->sourceNodeId = edge.sourceNodeId;
    probe->sourcePortId = edge.sourcePortId;
    probe->anchorDestNodeId = edge.destNodeId;
    probe->anchorDestPortId = edge.destPortId;
    probe->tapPosition = jlimit(0.f, 1.f, tapPosition);
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, probeId, {} };
    result.changes.probesChanged = true;
    return result;
}

GraphEditResult GraphEditor::spliceNodeIntoEdge(NodeGraph& graph, size_t edgeIndex, const String& nodeId) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    const Edge edge = graph.getEdges()[edgeIndex];

    if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const Node* spliceNode = findNode(graph, nodeId);

    if (spliceNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    const PortAddress source { edge.sourceNodeId, edge.sourcePortId, false };
    const PortAddress dest { edge.destNodeId, edge.destPortId, true };

    for (const auto& input : spliceNode->inputs) {
        if (!input.input) {
            continue;
        }

        for (const auto& output : spliceNode->outputs) {
            if (output.input) {
                continue;
            }

            NodeGraph candidate = graph;
            candidate.removeEdgeAt(edgeIndex);

            GraphEditResult inResult = connect(candidate, source, { nodeId, input.id, true });

            if (!inResult.succeeded()) {
                continue;
            }

            GraphEditResult outResult = connect(candidate, { nodeId, output.id, false }, dest);

            if (!outResult.succeeded()) {
                continue;
            }

            graph = std::move(candidate);
            GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
            result.changes.nodeIds.push_back(nodeId);
            result.changes.topologyChanged = true;
            return result;
        }
    }

    return { GraphEditCode::ValidationRejected, {}, {} };
}

GraphEditResult GraphEditor::removeNode(NodeGraph& graph, const String& nodeId) const {
    const Node* node = findNode(graph, nodeId);
    if (node == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }
    const auto* definition = NodeDefinitionRegistry::instance().find(node->kind);
    if (definition != nullptr && !definition->removable) {
        return { GraphEditCode::ValidationRejected, nodeId, {} };
    }

    const NodeAudioResourceBinding* binding = graph.findAudioResourceBinding(nodeId);
    const String resourceId = binding != nullptr ? binding->resourceId : String();
    graph.removeNode(nodeId);
    if (resourceId.isNotEmpty() && graph.audioResourceUsageCount(resourceId) == 0) {
        graph.removeAudioResource(resourceId);
    }
    GraphEditResult result;
    result.changes.nodeIds.push_back(nodeId);
    result.changes.topologyChanged = true;
    return result;
}

GraphEditResult GraphEditor::removeEdgeAt(NodeGraph& graph, size_t index) const {
    if (index >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    graph.removeEdgeAt(index);
    GraphEditResult result;
    result.changes.topologyChanged = true;
    return result;
}

const Node* GraphEditor::findNode(const NodeGraph& graph, const String& nodeId) const {
    return graph.findNode(nodeId);
}

const Port* GraphEditor::findPort(const Node& node, const String& portId, bool input) const {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

String GraphEditor::createUniqueNodeId(const NodeGraph& graph, NodeKind kind) const {
    const String baseId = baseIdForKind(kind);
    String candidate = baseId;
    int suffix = 2;

    while (findNode(graph, candidate) != nullptr) {
        candidate = baseId + String(suffix);
        ++suffix;
    }

    return candidate;
}

String GraphEditor::createUniqueProbeId(const NodeGraph& graph) const {
    String candidate = "probe";
    int suffix = 2;

    while (graph.findSignalProbe(candidate) != nullptr) {
        candidate = "probe" + String(suffix);
        ++suffix;
    }

    return candidate;
}

String GraphEditor::baseIdForKind(NodeKind kind) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    return definition != nullptr ? definition->defaultInstanceIdPrefix : "processor";
}

}
