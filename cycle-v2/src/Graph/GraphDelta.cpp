#include "Graph/GraphDelta.h"

#include <algorithm>

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
        && guides.empty();
}

void GraphDelta::apply(NodeGraph& graph, bool forward) const {
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
    for (const auto& delta : guides) {
        graph.applyGuideCurveState(forward ? delta.after : delta.before);
    }
    if (forward) {
        for (const auto& edge : changes.removedEdges) {
            graph.removeEdge(edge);
        }
        for (const auto& assignment : changes.removedGuideAssignments) {
            graph.removeGuideAssignment(assignment.targetNodeId, assignment.target);
        }
    } else {
        for (const auto& edge : changes.removedEdges) {
            graph.addEdge(edge);
        }
        for (const auto& assignment : changes.removedGuideAssignments) {
            graph.assignGuideCurve(assignment);
        }
    }
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

GraphDelta GraphDeltaBuilder::finish(
        const NodeGraph& graph,
        GraphChangeSet changes) const {
    GraphDelta result;
    result.parameters = parameters;
    result.models = models;
    result.editorStates = editorStates;
    result.bounds = bounds;
    result.guides = guides;
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
    for (auto& delta : result.guides) {
        const GuideCurveResource* guide = graph.findGuideCurve(delta.guideId);
        if (guide != nullptr) {
            delta.after = *guide;
        }
    }
    return result;
}

void GraphDeltaBuilder::restore(NodeGraph& graph) const {
    GraphDelta delta = finish(graph, {});
    delta.applyInverse(graph);
}

bool GraphDeltaBuilder::empty() const {
    return parameters.empty()
        && models.empty()
        && editorStates.empty()
        && bounds.empty()
        && guides.empty();
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
