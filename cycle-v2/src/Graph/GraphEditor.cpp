#include <algorithm>
#include <utility>

#include "Graph/GraphConnectionValidator.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphSpliceValidator.h"
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
    auto validation = GraphSpliceValidator().validate(graph, edgeIndex, nodeId);
    if (!validation.succeeded()) {
        return { validation.code, {}, {} };
    }

    graph.removeEdgeAt(edgeIndex);
    graph.removeEdgesToInput(
            validation.incomingEdge.destNodeId,
            validation.incomingEdge.destPortId);
    graph.addEdge(std::move(validation.incomingEdge));
    graph.removeEdgesToInput(
            validation.outgoingEdge.destNodeId,
            validation.outgoingEdge.destPortId);
    graph.addEdge(std::move(validation.outgoingEdge));
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.topologyChanged = true;
    return result;
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

std::optional<CableDeletionPlan> GraphEditor::planCableDeletion(
        const NodeGraph& graph,
        const std::vector<size_t>& edgeIndices) const {
    if (edgeIndices.empty()) {
        return std::nullopt;
    }
    for (const size_t index : edgeIndices) {
        if (index >= graph.getEdges().size()) {
            return std::nullopt;
        }
    }

    const Edge& selected = graph.getEdges()[edgeIndices.front()];
    const Node* source = graph.findNode(selected.sourceNodeId);
    const Node* destination = graph.findNode(selected.destNodeId);
    const String panNodeId = source != nullptr && source->kind == NodeKind::SpectralLayer
            ? source->id
            : (destination != nullptr && destination->kind == NodeKind::SpectralLayer
                    ? destination->id : String());
    CableDeletionPlan plan { panNodeId, edgeIndices, {} };
    if (panNodeId.isNotEmpty()) {
        plan.edgeIndices.clear();
        for (size_t index = 0; index < graph.getEdges().size(); ++index) {
            const Edge& edge = graph.getEdges()[index];
            if (edge.sourceNodeId == panNodeId || edge.destNodeId == panNodeId) {
                plan.edgeIndices.push_back(index);
            }
        }
    }

    for (const auto& probe : graph.getSignalProbes()) {
        const bool attached = std::any_of(
                plan.edgeIndices.begin(), plan.edgeIndices.end(), [&](size_t index) {
                    const Edge& edge = graph.getEdges()[index];
                    return probe.sourceNodeId == edge.sourceNodeId
                            && probe.sourcePortId == edge.sourcePortId
                            && probe.anchorDestNodeId == edge.destNodeId
                            && probe.anchorDestPortId == edge.destPortId;
                });
        if (attached) {
            plan.probeIds.push_back(probe.id);
        }
    }
    return plan;
}

GraphEditResult GraphEditor::removeCable(
        NodeGraph& graph,
        const CableDeletionPlan& plan) const {
    for (const auto& probeId : plan.probeIds) {
        graph.removeSignalProbe(probeId);
    }
    if (plan.panNodeId.isNotEmpty()) {
        graph.removeNode(plan.panNodeId);
    } else {
        std::vector<size_t> indices = plan.edgeIndices;
        std::sort(indices.begin(), indices.end(), std::greater<size_t>());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        for (const size_t index : indices) {
            graph.removeEdgeAt(index);
        }
    }

    GraphEditResult result;
    result.changes.probesChanged = !plan.probeIds.empty();
    if (plan.panNodeId.isNotEmpty()) {
        result.changes.nodeIds.push_back(plan.panNodeId);
    }
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
