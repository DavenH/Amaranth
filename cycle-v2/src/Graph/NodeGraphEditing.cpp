#include "Graph/NodeGraph.h"

#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

namespace {

bool editorStatesEqual(const var& first, const var& second) {
    const Array<var>* firstArray = first.getArray();
    const Array<var>* secondArray = second.getArray();
    if (firstArray != nullptr || secondArray != nullptr) {
        if (firstArray == nullptr || secondArray == nullptr
                || firstArray->size() != secondArray->size()) {
            return false;
        }
        for (int index = 0; index < firstArray->size(); ++index) {
            if (!editorStatesEqual((*firstArray)[index], (*secondArray)[index])) {
                return false;
            }
        }
        return true;
    }

    const DynamicObject* firstObject = first.getDynamicObject();
    const DynamicObject* secondObject = second.getDynamicObject();
    if (firstObject != nullptr || secondObject != nullptr) {
        if (firstObject == nullptr || secondObject == nullptr) {
            return false;
        }
        const NamedValueSet& firstProperties = firstObject->getProperties();
        const NamedValueSet& secondProperties = secondObject->getProperties();
        if (firstProperties.size() != secondProperties.size()) {
            return false;
        }
        for (int index = 0; index < firstProperties.size(); ++index) {
            const Identifier name = firstProperties.getName(index);
            if (!secondProperties.contains(name)
                    || !editorStatesEqual(
                            firstProperties.getValueAt(index),
                            secondProperties[name])) {
                return false;
            }
        }
        return true;
    }

    return first.equalsWithSameType(second);
}

}

NodeGraph::NodeGraph(const NodeGraph& other) {
    *this = other;
}

NodeGraph& NodeGraph::operator=(const NodeGraph& other) {
    if (this == &other) {
        return *this;
    }

    InteractionComplexityDiagnostics::recordGraphCopy();
    nodes = other.nodes;
    edges = other.edges;
    guideCurves = other.guideCurves;
    guideHeatmaps = other.guideHeatmaps;
    guideAssignments = other.guideAssignments;
    signalProbes = other.signalProbes;
    audioResources = other.audioResources;
    audioResourceBindings = other.audioResourceBindings;
    nodeIndex = other.nodeIndex;
    parameterIndices = other.parameterIndices;
    guideResourceIndex = other.guideResourceIndex;
    guideHeatmapIndex = other.guideHeatmapIndex;
    guideAssignmentTargetIndex = other.guideAssignmentTargetIndex;
    guideUsageCounts = other.guideUsageCounts;
    guideTargetNodes = other.guideTargetNodes;
    targetNodeGuides = other.targetNodeGuides;
    overlayNodeView = other.overlayNodeView;
    overlayGuideView = other.overlayGuideView;
    overlayNodeViewRevision = other.overlayNodeViewRevision;
    overlayGuideViewRevision = other.overlayGuideViewRevision;
    overlayBase = other.overlayBase;
    revision = other.revision;
    return *this;
}

NodeGraph NodeGraph::createEditingOverlay(const NodeGraph& base) {
    NodeGraph result;
    result.overlayBase = &base;
    result.revision = base.revision;
    return result;
}

const std::vector<Node>& NodeGraph::getNodes() const {
    if (overlayBase == nullptr) {
        return nodes;
    }
    if (overlayNodeViewRevision == revision) {
        return overlayNodeView;
    }

    overlayNodeView = overlayBase->getNodes();
    for (const auto& node : nodes) {
        const auto baseNode = overlayBase->nodeIndex.find(node.id);
        if (baseNode != overlayBase->nodeIndex.end()
                && baseNode->second < overlayNodeView.size()) {
            overlayNodeView[baseNode->second] = node;
        } else {
            overlayNodeView.push_back(node);
        }
    }
    overlayNodeViewRevision = revision;
    return overlayNodeView;
}

const std::vector<GuideCurveResource>& NodeGraph::getGuideCurves() const {
    if (overlayBase == nullptr) {
        return guideCurves;
    }
    if (overlayGuideViewRevision == revision) {
        return overlayGuideView;
    }

    overlayGuideView = overlayBase->getGuideCurves();
    for (const auto& guide : guideCurves) {
        const auto baseGuide = overlayBase->guideResourceIndex.find(guide.id);
        if (baseGuide != overlayBase->guideResourceIndex.end()
                && baseGuide->second < overlayGuideView.size()) {
            overlayGuideView[baseGuide->second] = guide;
        } else {
            overlayGuideView.push_back(guide);
        }
    }
    overlayGuideViewRevision = revision;
    return overlayGuideView;
}

const Node* NodeGraph::findNode(const String& nodeId) const {
    const auto found = nodeIndex.find(nodeId);
    const Node* local = found != nodeIndex.end() && found->second < nodes.size()
            ? &nodes[found->second]
            : nullptr;
    return local != nullptr || overlayBase == nullptr
            ? local
            : overlayBase->findNode(nodeId);
}

Node* NodeGraph::findNodeForEditing(const String& nodeId) {
    const auto found = nodeIndex.find(nodeId);
    if (found != nodeIndex.end() && found->second < nodes.size()) {
        return &nodes[found->second];
    }
    const Node* baseNode = overlayBase != nullptr ? overlayBase->findNode(nodeId) : nullptr;
    if (baseNode == nullptr) {
        return nullptr;
    }

    nodes.push_back(*baseNode);
    nodeIndex[nodeId] = nodes.size() - 1;
    rebuildParameterIndex(nodeId);
    return &nodes.back();
}

const NodeParameter* NodeGraph::findNodeParameter(
        const String& nodeId,
        const String& parameterId) const {
    const Node* node = findNode(nodeId);
    const auto nodeParameters = parameterIndices.find(nodeId);
    if (nodeParameters == parameterIndices.end()) {
        return overlayBase != nullptr
                ? overlayBase->findNodeParameter(nodeId, parameterId)
                : nullptr;
    }
    const auto found = nodeParameters->second.find(parameterId);
    return found != nodeParameters->second.end() && found->second < node->parameters.size()
            ? &node->parameters[found->second]
            : nullptr;
}

NodeParameter* NodeGraph::findNodeParameterForEditing(
        const String& nodeId,
        const String& parameterId) {
    Node* node = findNodeForEditing(nodeId);
    const auto nodeParameters = parameterIndices.find(nodeId);
    if (node == nullptr || nodeParameters == parameterIndices.end()) {
        return nullptr;
    }
    const auto found = nodeParameters->second.find(parameterId);
    return found != nodeParameters->second.end() && found->second < node->parameters.size()
            ? &node->parameters[found->second]
            : nullptr;
}

bool NodeGraph::replaceNodeParameters(
        const String& nodeId,
        std::vector<NodeParameter> parameters) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    node->parameters = std::move(parameters);
    rebuildParameterIndex(nodeId);
    ++revision;
    return true;
}

bool NodeGraph::addNodeParameter(const String& nodeId, NodeParameter parameter) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr || findNodeParameter(nodeId, parameter.id) != nullptr) {
        return false;
    }

    node->parameters.push_back(std::move(parameter));
    parameterIndices[nodeId][node->parameters.back().id] = node->parameters.size() - 1;
    ++revision;
    return true;
}

bool NodeGraph::replaceNodeModel(const String& nodeId, NodeModelStatePtr model) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    if ((node->model == nullptr && model == nullptr)
            || (node->model != nullptr && model != nullptr && node->model->equals(*model))) {
        return false;
    }

    node->model = std::move(model);
    markChanged();
    return true;
}

bool NodeGraph::replaceNodeEditorState(const String& nodeId, var editorState) {
    InteractionComplexityDiagnostics::recordEditorStateComparison();
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr || editorStatesEqual(node->editorState, editorState)) {
        return false;
    }

    node->editorState = std::move(editorState);
    markChanged();
    return true;
}

bool NodeGraph::setNodeBounds(const String& nodeId, Rectangle<float> bounds) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    node->bounds = bounds;
    ++revision;
    return true;
}

void NodeGraph::translateNodes(
        const std::vector<String>& nodeIds,
        Point<float> offset) {
    bool changed = false;
    for (const auto& nodeId : nodeIds) {
        if (Node* node = findNodeForEditing(nodeId)) {
            node->bounds = node->bounds.translated(offset.x, offset.y);
            changed = true;
        }
    }
    if (changed) {
        ++revision;
    }
}

void NodeGraph::rebuildNodeIndex() {
    nodeIndex.clear();
    parameterIndices.clear();
    nodeIndex.reserve(nodes.size());
    parameterIndices.reserve(nodes.size());
    for (size_t index = 0; index < nodes.size(); ++index) {
        nodeIndex[nodes[index].id] = index;
        rebuildParameterIndex(nodes[index].id);
    }
}

void NodeGraph::rebuildParameterIndex(const String& nodeId) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        parameterIndices.erase(nodeId);
        return;
    }

    auto& index = parameterIndices[nodeId];
    index.clear();
    index.reserve(node->parameters.size());
    for (size_t parameterIndex = 0; parameterIndex < node->parameters.size(); ++parameterIndex) {
        index[node->parameters[parameterIndex].id] = parameterIndex;
    }
}

void NodeGraph::applyNodeParameterState(
        const String& nodeId,
        const String& parameterId,
        const std::optional<NodeParameter>& state) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return;
    }

    const auto found = parameterIndices[nodeId].find(parameterId);
    if (state.has_value()) {
        if (found != parameterIndices[nodeId].end()) {
            node->parameters[found->second] = *state;
        } else {
            node->parameters.push_back(*state);
        }
    } else if (found != parameterIndices[nodeId].end()) {
        node->parameters.erase(node->parameters.begin() + (int) found->second);
    }
    rebuildParameterIndex(nodeId);
    ++revision;
}

void NodeGraph::applyNodeModelState(const String& nodeId, NodeModelStatePtr state) {
    if (Node* node = findNodeForEditing(nodeId)) {
        node->model = std::move(state);
        ++revision;
    }
}

void NodeGraph::applyNodeEditorState(const String& nodeId, var state) {
    if (Node* node = findNodeForEditing(nodeId)) {
        node->editorState = std::move(state);
        ++revision;
    }
}

void NodeGraph::applyNodeBounds(const String& nodeId, Rectangle<float> state) {
    if (Node* node = findNodeForEditing(nodeId)) {
        node->bounds = state;
        ++revision;
    }
}

void NodeGraph::applyGuideCurveState(GuideCurveResource state) {
    GuideCurveResource* guide = findGuideCurveForEditing(state.id);
    if (guide != nullptr) {
        *guide = std::move(state);
        ++revision;
    }
}

}
