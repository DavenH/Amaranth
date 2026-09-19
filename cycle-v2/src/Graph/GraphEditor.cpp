#include <algorithm>
#include <utility>

#include "Graph/GraphConnectionValidator.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidator.h"

namespace CycleV2 {

namespace {

bool isProbeDomain(PortDomain domain) {
    return domain == PortDomain::TimeSignal
            || domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
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
    auto validation = GraphConnectionValidator().validate(graph, first, second);
    if (!validation.succeeded()) {
        return { validation.code, {}, std::move(validation.issues) };
    }

    graph.removeEdgesToInput(
            validation.destination.nodeId,
            validation.destination.portId);
    graph.addEdge(std::move(validation.edge));
    GraphEditResult result;
    result.changes.nodeIds = {
            validation.source.nodeId,
            validation.destination.nodeId
    };
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
    const GraphConnectionValidator connectionValidator;
    const GraphValidator validator;
    const GraphEdgeView withoutOriginal(graph.getEdges(), { edgeIndex }, {});
    const auto removedEdgeIssues = validator.validate(graph, withoutOriginal);

    for (const auto& input : spliceNode->inputs) {
        if (!input.input) {
            continue;
        }

        auto incoming = connectionValidator.propose(
                graph, source, { nodeId, input.id, true });
        if (!incoming.succeeded()) {
            continue;
        }
        auto firstRemoved = connectionValidator.edgeIndicesToInput(
                graph, incoming.destination);
        firstRemoved.push_back(edgeIndex);
        const GraphEdgeView firstProposal(
                graph.getEdges(), firstRemoved, { incoming.edge });
        const auto firstIssues = validator.validate(graph, firstProposal);
        if (!firstIssues.empty()
                && !GraphValidator::acceptsProposedIssues(removedEdgeIssues, firstIssues)) {
            continue;
        }

        for (const auto& output : spliceNode->outputs) {
            if (output.input) {
                continue;
            }

            auto outgoing = connectionValidator.propose(
                    graph, { nodeId, output.id, false }, dest);
            if (!outgoing.succeeded()) {
                continue;
            }

            auto finalRemoved = firstRemoved;
            const auto replacedDestination = connectionValidator.edgeIndicesToInput(
                    graph, outgoing.destination);
            finalRemoved.insert(
                    finalRemoved.end(),
                    replacedDestination.begin(),
                    replacedDestination.end());
            const GraphEdgeView finalProposal(
                    graph.getEdges(),
                    std::move(finalRemoved),
                    { incoming.edge, outgoing.edge });
            const auto finalIssues = validator.validate(graph, finalProposal);
            if (!finalIssues.empty()
                    && !GraphValidator::acceptsProposedIssues(firstIssues, finalIssues)) {
                continue;
            }

            graph.removeEdgeAt(edgeIndex);
            graph.removeEdgesToInput(nodeId, input.id);
            graph.addEdge(std::move(incoming.edge));
            graph.removeEdgesToInput(dest.nodeId, dest.portId);
            graph.addEdge(std::move(outgoing.edge));
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
