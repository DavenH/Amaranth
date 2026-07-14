#include "GraphCommandDispatcher.h"

namespace CycleV2 {

GraphEditResult GraphCommandDispatcher::addNode(NodeKind kind, juce::Point<float> position) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().addNode(graph, kind, position);
        if (result.succeeded()) {
            result.changes.nodeIds.push_back(result.nodeId);
            result.changes.topologyChanged = true;
            result.changes.layoutChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::removeNode(const juce::String& nodeId) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().removeNode(graph, nodeId);
        if (result.succeeded()) {
            result.changes.nodeIds.push_back(nodeId);
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::removeEdgeAt(size_t edgeIndex) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().removeEdgeAt(graph, edgeIndex);
        if (result.succeeded()) {
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::connect(const PortAddress& first, const PortAddress& second) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().connect(graph, first, second);
        if (result.succeeded()) {
            result.changes.nodeIds = { first.nodeId, second.nodeId };
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::attachSpyToEdge(
        size_t edgeIndex,
        const juce::String& spyNodeId) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().attachSpyToEdge(graph, edgeIndex, spyNodeId);
        if (result.succeeded()) {
            result.changes.nodeIds.push_back(spyNodeId);
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::spliceNodeIntoEdge(
        size_t edgeIndex,
        const juce::String& nodeId) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().spliceNodeIntoEdge(graph, edgeIndex, nodeId);
        if (result.succeeded()) {
            result.changes.nodeIds.push_back(nodeId);
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::attachGuideCurve(
        const juce::String& guideNodeId,
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().attachGuideCurveToTrimeshVertexParameter(
                graph, guideNodeId, meshNodeId, vertexIndex, parameterField);
        if (result.succeeded()) {
            result.changes.nodeIds = { guideNodeId, meshNodeId };
            result.changes.topologyChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::createAndAttachGuideCurve(
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField,
        juce::Point<float> guidePosition) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().createAndAttachGuideCurveToTrimeshVertexParameter(
                graph, meshNodeId, vertexIndex, parameterField, guidePosition);
        if (result.succeeded()) {
            result.changes.nodeIds.push_back(meshNodeId);
            result.changes.topologyChanged = true;
            result.changes.layoutChanged = true;
        }
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::setNodeParameter(
        const juce::String& nodeId,
        const juce::String& parameterId,
        const juce::String& label,
        const juce::String& value) {
    return apply([&](auto& graph) {
        return GraphEditor().setNodeParameter(graph, nodeId, parameterId, label, value);
    });
}

GraphEditResult GraphCommandDispatcher::moveNode(
        const juce::String& nodeId,
        juce::Point<float> position) {
    const Node* node = document.graph().findNode(nodeId);
    if (node == nullptr) {
        GraphEditResult result;
        result.code = GraphEditCode::MissingNode;
        return result;
    }
    return setNodeBounds(nodeId, node->bounds.withPosition(position));
}

GraphEditResult GraphCommandDispatcher::resizeNode(
        const juce::String& nodeId,
        juce::Rectangle<float> bounds) {
    return setNodeBounds(nodeId, bounds);
}

GraphEditResult GraphCommandDispatcher::editNodePresentation(
        const juce::String& nodeId,
        const std::function<void(Node&)>& edit) {
    return apply([&](auto& graph) {
        GraphEditResult result;
        Node* node = graph.findNodeForEditing(nodeId);
        if (node == nullptr) {
            result.code = GraphEditCode::MissingNode;
            return result;
        }
        edit(*node);
        graph.markChanged();
        result.nodeId = nodeId;
        result.changes.nodeIds.push_back(nodeId);
        result.changes.layoutChanged = true;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::translateNodes(
        const std::vector<juce::String>& nodeIds,
        juce::Point<float> offset) {
    return apply([&](auto& graph) {
        graph.translateNodes(nodeIds, offset);
        GraphEditResult result;
        result.changes.nodeIds = nodeIds;
        result.changes.layoutChanged = true;
        return result;
    });
}

void GraphCommandDispatcher::beginCompoundEdit() {
    if (compoundActive) {
        ++compoundDepth;
        return;
    }
    compoundBefore = document.toXml();
    compoundActive = true;
    compoundChanged = false;
    compoundDepth = 1;
}

void GraphCommandDispatcher::commitCompoundEdit() {
    if (!compoundActive) {
        return;
    }
    if (--compoundDepth > 0) {
        return;
    }
    if (compoundChanged) {
        document.recordBeforeChange(std::move(compoundBefore));
    }
    compoundBefore = {};
    compoundActive = false;
    compoundChanged = false;
    compoundDepth = 0;
}

void GraphCommandDispatcher::cancelCompoundEdit() {
    if (compoundActive && compoundChanged) {
        document.restoreXml(compoundBefore);
    }
    compoundBefore = {};
    compoundActive = false;
    compoundChanged = false;
    compoundDepth = 0;
}

GraphEditResult GraphCommandDispatcher::apply(
        const std::function<GraphEditResult(NodeGraph&)>& command) {
    const juce::String before = compoundActive ? juce::String() : document.toXml();
    GraphEditResult result = command(document.graphForCommand());
    if (!result.succeeded()) {
        return result;
    }

    if (compoundActive) {
        compoundChanged = true;
    } else {
        document.recordBeforeChange(before);
    }
    document.publishChange(result.changes);
    return result;
}

GraphEditResult GraphCommandDispatcher::setNodeBounds(
        const juce::String& nodeId,
        juce::Rectangle<float> bounds) {
    return apply([&](auto& graph) {
        GraphEditResult result;
        if (!graph.setNodeBounds(nodeId, bounds)) {
            result.code = GraphEditCode::MissingNode;
            return result;
        }
        result.nodeId = nodeId;
        result.changes.nodeIds.push_back(nodeId);
        result.changes.layoutChanged = true;
        return result;
    });
}

}
