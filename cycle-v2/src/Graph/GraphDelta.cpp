#include "Graph/GraphDelta.h"

#include <algorithm>

#include "Graph/GraphEditor.h"
#include "Graph/GraphGuideIndex.h"

namespace CycleV2 {

namespace {

template<typename Delta>
bool containsAddress(
        const std::vector<Delta>& deltas,
        const String& id) {
    return std::any_of(deltas.begin(), deltas.end(), [&](const Delta& delta) {
        return delta.nodeId == id;
    });
}

bool containsParameterAddress(
        const std::vector<NodeParameterDelta>& deltas,
        const String& nodeId,
        const String& parameterId) {
    return std::any_of(deltas.begin(), deltas.end(), [&](const auto& delta) {
        return delta.nodeId == nodeId && delta.parameterId == parameterId;
    });
}

const NodeParameter* parameter(
        const NodeGraph& graph,
        const String& nodeId,
        const String& parameterId) {
    return graph.findNodeParameter(nodeId, parameterId);
}

std::vector<IndexedEdgeState> edgesToInput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId) {
    std::vector<IndexedEdgeState> result;
    for (size_t index = 0; index < graph.getEdges().size(); ++index) {
        const Edge& edge = graph.getEdges()[index];
        if (edge.destNodeId == nodeId && edge.destPortId == portId) {
            result.push_back({ index, edge });
        }
    }
    return result;
}

}

void GraphDelta::applyForward(NodeGraph& graph) const {
    apply(graph, true);
}

void GraphDelta::applyInverse(NodeGraph& graph) const {
    apply(graph, false);
}

bool GraphDelta::empty() const {
    return parameters.empty()
        && models.empty()
        && editorStates.empty()
        && bounds.empty()
        && nodeGroups.empty()
        && probePositions.empty()
        && guides.empty()
        && routing.empty();
}

void GraphDelta::apply(NodeGraph& graph, bool forward) const {
    if (!forward) {
        for (auto delta = routing.rbegin(); delta != routing.rend(); ++delta) {
            applyRouting(graph, *delta, false);
        }
    }
    for (const auto& delta : parameters) {
        graph.applyNodeParameterState(
                delta.nodeId,
                delta.parameterId,
                forward ? delta.after : delta.before);
    }
    for (const auto& delta : models) {
        graph.applyNodeModelState(delta.nodeId, forward ? delta.after : delta.before);
    }
    for (const auto& delta : editorStates) {
        graph.applyNodeEditorState(delta.nodeId, forward ? delta.after : delta.before);
    }
    for (const auto& delta : bounds) {
        graph.applyNodeBounds(delta.nodeId, forward ? delta.after : delta.before);
    }
    for (const auto& delta : nodeGroups) {
        const auto& state = forward ? delta.after : delta.before;
        if (!state.has_value()) {
            graph.removeNodeGroup(delta.groupId);
        } else if (!graph.replaceNodeGroup(*state)) {
            graph.addNodeGroup(*state);
        }
    }
    for (const auto& delta : probePositions) {
        graph.setSignalProbeCanvasPosition(
                delta.probeId,
                forward ? delta.after : delta.before);
    }
    for (const auto& delta : guides) {
        graph.applyGuideCurveState(forward ? delta.after : delta.before);
    }
    if (forward) {
        for (const auto& assignment : changes.removedGuideAssignments) {
            graph.removeGuideAssignment(
                    assignment.assignment.targetNodeId, assignment.assignment.target);
        }
        for (const auto& assignment : changes.addedGuideAssignments) {
            graph.assignGuideCurve(assignment);
        }
    } else {
        for (const auto& assignment : changes.addedGuideAssignments) {
            graph.removeGuideAssignment(assignment.targetNodeId, assignment.target);
        }
        for (auto assignment = changes.removedGuideAssignments.rbegin();
                assignment != changes.removedGuideAssignments.rend(); ++assignment) {
            const size_t index = std::min(assignment->index, graph.guideAssignments.size());
            graph.guideAssignments.insert(
                    graph.guideAssignments.begin() + (int) index, assignment->assignment);
            ++graph.revision;
        }
        if (!changes.removedGuideAssignments.empty()) {
            graph.guideIndex->rebuildAssignments(graph.guideAssignments);
        }
    }
    if (forward) {
        for (const auto& delta : routing) {
            applyRouting(graph, delta, true);
        }
    }
}

void GraphDelta::applyCableDeletion(
        NodeGraph& graph,
        const CableDeletionDelta& deletion,
        bool forward) {
    if (forward) {
        for (const auto& state : deletion.probes) {
            graph.removeSignalProbe(state.probe.id);
        }
        for (auto edge = deletion.edges.rbegin(); edge != deletion.edges.rend(); ++edge) {
            graph.removeEdgeAt(edge->index);
        }
        if (deletion.pan.has_value()) {
            graph.removeNode(deletion.pan->node.id);
        }
        return;
    }

    if (deletion.pan.has_value()) {
        const size_t index = std::min(deletion.pan->index, graph.nodes.size());
        graph.nodes.insert(graph.nodes.begin() + (int) index, deletion.pan->node);
        graph.rebuildNodeIndex();
        ++graph.revision;
    }
    for (const auto& state : deletion.edges) {
        const size_t index = std::min(state.index, graph.edges.size());
        graph.edges.insert(graph.edges.begin() + (int) index, state.edge);
        ++graph.revision;
    }
    for (const auto& state : deletion.probes) {
        const size_t index = std::min(state.index, graph.signalProbes.size());
        graph.signalProbes.insert(graph.signalProbes.begin() + (int) index, state.probe);
        ++graph.revision;
    }
}

void GraphDelta::applyRouting(NodeGraph& graph, const RoutingDelta& delta, bool forward) {
    if (const auto* deletion = std::get_if<CableDeletionDelta>(&delta)) {
        applyCableDeletion(graph, *deletion, forward);
        return;
    }
    const auto& edges = std::get<EdgeRoutingDelta>(delta);
    auto source = forward ? edges.before : edges.after;
    auto target = forward ? edges.after : edges.before;
    std::sort(source.begin(), source.end(), [](const auto& left, const auto& right) {
        return left.index > right.index;
    });
    for (const auto& state : source) {
        jassert(state.index < graph.edges.size());
        if (state.index < graph.edges.size()) {
            graph.edges.erase(graph.edges.begin() + (int) state.index);
        }
    }
    std::sort(target.begin(), target.end(), [](const auto& left, const auto& right) {
        return left.index < right.index;
    });
    for (const auto& state : target) {
        const size_t index = std::min(state.index, graph.edges.size());
        graph.edges.insert(graph.edges.begin() + (int) index, state.edge);
    }
    ++graph.revision;
}

void GraphDeltaBuilder::captureNodeParameter(
        const NodeGraph& graph,
        const String& nodeId,
        const String& parameterId) {
    if (containsParameterAddress(parameters, nodeId, parameterId)) {
        return;
    }
    const NodeParameter* before = parameter(graph, nodeId, parameterId);
    parameters.push_back({
            nodeId,
            parameterId,
            before != nullptr ? std::optional<NodeParameter>(*before) : std::nullopt,
            {}
    });
    const Node* node = graph.findNode(nodeId);
    if (node != nullptr && node->kind == NodeKind::Envelope && parameterId == "purpose") {
        captureNodeParameter(graph, nodeId, "logarithmic");
    }
}

void GraphDeltaBuilder::captureNodeModel(const NodeGraph& graph, const String& nodeId) {
    if (containsAddress(models, nodeId)) {
        return;
    }
    const Node* node = graph.findNode(nodeId);
    if (node != nullptr) {
        models.push_back({ nodeId, node->model, {} });
    }
}

void GraphDeltaBuilder::captureNodeEditorState(
        const NodeGraph& graph,
        const String& nodeId) {
    if (containsAddress(editorStates, nodeId)) {
        return;
    }
    const Node* node = graph.findNode(nodeId);
    if (node != nullptr) {
        editorStates.push_back({ nodeId, node->editorState, {} });
    }
}

void GraphDeltaBuilder::captureNodeBounds(const NodeGraph& graph, const String& nodeId) {
    if (containsAddress(bounds, nodeId)) {
        return;
    }
    const Node* node = graph.findNode(nodeId);
    if (node != nullptr) {
        bounds.push_back({ nodeId, node->bounds, {} });
    }
}

void GraphDeltaBuilder::captureNodeGroup(const NodeGraph& graph, const String& groupId) {
    const auto found = std::find_if(nodeGroups.begin(), nodeGroups.end(),
            [&](const NodeGroupDelta& delta) { return delta.groupId == groupId; });
    if (found != nodeGroups.end()) {
        return;
    }
    const NodeGroup* group = graph.findNodeGroup(groupId);
    nodeGroups.push_back({ groupId,
            group != nullptr ? std::optional<NodeGroup>(*group) : std::nullopt,
            std::nullopt });
}

void GraphDeltaBuilder::captureSignalProbePosition(
        const NodeGraph& graph,
        const String& probeId) {
    const auto found = std::find_if(probePositions.begin(), probePositions.end(),
            [&](const auto& delta) { return delta.probeId == probeId; });
    if (found != probePositions.end()) {
        return;
    }
    if (const SignalProbe* probe = graph.findSignalProbe(probeId)) {
        probePositions.push_back({ probeId, probe->canvasPosition, {} });
    }
}

void GraphDeltaBuilder::captureGuideCurve(const NodeGraph& graph, const String& guideId) {
    const bool alreadyCaptured = std::any_of(
            guides.begin(),
            guides.end(),
            [&](const auto& delta) { return delta.guideId == guideId; });
    const GuideCurveResource* guide = graph.findGuideCurve(guideId);
    if (!alreadyCaptured && guide != nullptr) {
        guides.push_back({ guideId, *guide, {} });
    }
}

void GraphDeltaBuilder::captureEdgesToInput(
        const NodeGraph& graph,
        const String& nodeId,
        const String& portId) {
    const bool alreadyCaptured = std::any_of(
            edgeInputs.begin(), edgeInputs.end(), [&](const auto& delta) {
                return delta.nodeId == nodeId && delta.portId == portId;
            });
    if (!alreadyCaptured) {
        edgeInputs.push_back({ nodeId, portId, edgesToInput(graph, nodeId, portId) });
    }
}

void GraphDeltaBuilder::captureCableDeletion(
        const NodeGraph& graph,
        const CableDeletionPlan& plan) {
    CableDeletionDelta state;
    if (plan.panNodeId.isNotEmpty()) {
        if (const NodeGroup* group = graph.groupForNode(plan.panNodeId)) {
            captureNodeGroup(graph, group->id);
        }
        const auto& nodes = graph.getNodes();
        const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const Node& node) {
            return node.id == plan.panNodeId;
        });
        if (found != nodes.end()) {
            state.pan = { (size_t) std::distance(nodes.begin(), found), *found };
        }
    }
    for (const size_t index : plan.edgeIndices) {
        state.edges.push_back({ index, graph.getEdges()[index] });
    }
    std::sort(state.edges.begin(), state.edges.end(), [](const auto& left, const auto& right) {
        return left.index < right.index;
    });
    state.edges.erase(
            std::unique(state.edges.begin(), state.edges.end(),
                    [](const auto& left, const auto& right) {
                        return left.index == right.index;
                    }),
            state.edges.end());

    const auto& probes = graph.getSignalProbes();
    for (size_t index = 0; index < probes.size(); ++index) {
        if (std::find(plan.probeIds.begin(), plan.probeIds.end(), probes[index].id)
                != plan.probeIds.end()) {
            state.probes.push_back({ index, probes[index] });
        }
    }
    cableDeletions.push_back(std::move(state));
}

void GraphDeltaBuilder::finishCommand(
        const NodeGraph& graph, const GraphChangeSet& changes, bool changed) {
    if (changed) {
        EdgeRoutingDelta delta;
        for (const auto& input : edgeInputs) {
            delta.before.insert(delta.before.end(), input.before.begin(), input.before.end());
            const auto after = edgesToInput(graph, input.nodeId, input.portId);
            delta.after.insert(delta.after.end(), after.begin(), after.end());
        }
        if (!delta.before.empty() || !delta.after.empty()) {
            routing.emplace_back(std::move(delta));
        }
        for (auto& deletion : cableDeletions) {
            routing.emplace_back(std::move(deletion));
        }
        if (!changes.removedEdges.empty()) {
            routing.emplace_back(EdgeRoutingDelta { changes.removedEdges, {} });
        }
    }
    edgeInputs.clear();
    cableDeletions.clear();
}

GraphDelta GraphDeltaBuilder::finish(
        const NodeGraph& graph,
        GraphChangeSet changes) const {
    GraphDelta result;
    result.parameters = parameters;
    result.models = models;
    result.editorStates = editorStates;
    result.bounds = bounds;
    result.nodeGroups = nodeGroups;
    result.probePositions = probePositions;
    result.guides = guides;
    result.routing = routing;
    result.changes = std::move(changes);

    for (auto& delta : result.parameters) {
        const NodeParameter* after = parameter(graph, delta.nodeId, delta.parameterId);
        delta.after = after != nullptr ? std::optional<NodeParameter>(*after) : std::nullopt;
    }
    for (auto& delta : result.models) {
        const Node* node = graph.findNode(delta.nodeId);
        delta.after = node != nullptr ? node->model : NodeModelStatePtr();
    }
    for (auto& delta : result.editorStates) {
        const Node* node = graph.findNode(delta.nodeId);
        delta.after = node != nullptr ? node->editorState : var();
    }
    for (auto& delta : result.bounds) {
        const Node* node = graph.findNode(delta.nodeId);
        if (node != nullptr) {
            delta.after = node->bounds;
        }
    }
    for (auto& delta : result.nodeGroups) {
        const NodeGroup* group = graph.findNodeGroup(delta.groupId);
        delta.after = group != nullptr
                ? std::optional<NodeGroup>(*group) : std::nullopt;
    }
    for (auto& delta : result.probePositions) {
        const SignalProbe* probe = graph.findSignalProbe(delta.probeId);
        if (probe != nullptr) {
            delta.after = probe->canvasPosition;
        }
    }
    for (auto& delta : result.guides) {
        const GuideCurveResource* guide = graph.findGuideCurve(delta.guideId);
        if (guide != nullptr) {
            delta.after = *guide;
        }
    }
    return result;
}

void GraphDeltaBuilder::restore(NodeGraph& graph, GraphChangeSet changes) const {
    GraphDelta delta = finish(graph, std::move(changes));
    delta.applyInverse(graph);
}

bool GraphDeltaBuilder::empty() const {
    return parameters.empty()
        && models.empty()
        && editorStates.empty()
        && bounds.empty()
        && nodeGroups.empty()
        && probePositions.empty()
        && guides.empty()
        && edgeInputs.empty()
        && cableDeletions.empty()
        && routing.empty();
}

NodeModelStatePtr GraphDeltaBuilder::originalNodeModel(const String& nodeId) const {
    const auto found = std::find_if(models.begin(), models.end(), [&](const auto& delta) {
        return delta.nodeId == nodeId;
    });
    return found != models.end() ? found->before : NodeModelStatePtr();
}

const GuideCurveResource* GraphDeltaBuilder::originalGuideCurve(const String& guideId) const {
    const auto found = std::find_if(guides.begin(), guides.end(), [&](const auto& delta) {
        return delta.guideId == guideId;
    });
    return found != guides.end() ? &found->before : nullptr;
}

}
