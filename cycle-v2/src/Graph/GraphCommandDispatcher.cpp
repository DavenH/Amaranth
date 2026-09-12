#include "Graph/GraphCommandDispatcher.h"

#include <algorithm>

namespace CycleV2 {

namespace {

struct EditAnnotation {
    std::vector<juce::String> nodeIds;
    bool topologyChanged {};
    bool layoutChanged {};
};

std::vector<String> guideConsumerNodeIds(
        const NodeGraph& graph,
        const String& guideId) {
    return graph.guideTargetNodeIds(guideId);
}

GraphEditResult annotateSuccessful(
        GraphEditResult result,
        EditAnnotation annotation) {
    if (!result.succeeded()) {
        return result;
    }

    result.changes.nodeIds = std::move(annotation.nodeIds);
    result.changes.topologyChanged = annotation.topologyChanged;
    result.changes.layoutChanged = annotation.layoutChanged;
    return result;
}

}

GraphEditResult GraphCommandDispatcher::addNode(NodeKind kind, juce::Point<float> position) {
    return apply([&](auto& graph) {
        auto result = GraphEditor().addNode(graph, kind, position);
        const juce::String addedNodeId = result.nodeId;
        return annotateSuccessful(
                std::move(result),
                { { addedNodeId }, true, true });
    });
}

GraphEditResult GraphCommandDispatcher::removeNode(const juce::String& nodeId) {
    return apply([&](auto& graph) {
        return annotateSuccessful(
                GraphEditor().removeNode(graph, nodeId),
                { { nodeId }, true, false });
    });
}

GraphEditResult GraphCommandDispatcher::removeEdgeAt(size_t edgeIndex) {
    return apply([&](auto& graph) {
        return annotateSuccessful(
                GraphEditor().removeEdgeAt(graph, edgeIndex),
                { {}, true, false });
    });
}

GraphEditResult GraphCommandDispatcher::connect(const PortAddress& first, const PortAddress& second) {
    return apply([&](auto& graph) {
        return annotateSuccessful(
                GraphEditor().connect(graph, first, second),
                { { first.nodeId, second.nodeId }, true, false });
    });
}

GraphEditResult GraphCommandDispatcher::toggleSignalProbe(
        size_t edgeIndex,
        float tapPosition) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().toggleSignalProbe(graph, edgeIndex, tapPosition),
                { {}, false, false });
        result.changes.probesChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::removeSignalProbe(const juce::String& probeId) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().removeSignalProbe(graph, probeId),
                { {}, false, false });
        result.changes.probesChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::reattachSignalProbe(
        const juce::String& probeId,
        size_t edgeIndex,
        float tapPosition) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().reattachSignalProbe(graph, probeId, edgeIndex, tapPosition),
                { {}, false, false });
        result.changes.probesChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::spliceNodeIntoEdge(
        size_t edgeIndex,
        const juce::String& nodeId) {
    return apply([&](auto& graph) {
        return annotateSuccessful(
                GraphEditor().spliceNodeIntoEdge(graph, edgeIndex, nodeId),
                { { nodeId }, true, false });
    });
}

GraphEditResult GraphCommandDispatcher::createGuideCurve() {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().createGuideCurve(graph),
                { {}, false, false });
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::duplicateGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().duplicateGuideCurve(graph, guideId),
                { {}, false, false });
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::reorderGuideCurve(
        const juce::String& guideId,
        int shelfOrder) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().reorderGuideCurve(graph, guideId, shelfOrder),
                { {}, false, false });
        result.changes.guidePresentationChanged = result.succeeded() && result.changed;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::assignGuideCurve(
        const juce::String& guideId,
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().assignGuideCurveToTrimeshVertexParameter(
                        graph,
                        guideId,
                        meshNodeId,
                        vertexIndex,
                        parameterField),
                { { meshNodeId }, false, false });
        result.changes.guidesChanged = result.succeeded();
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::detachGuideCurve(
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().detachGuideCurveFromTrimeshVertexParameter(
                        graph, meshNodeId, vertexIndex, parameterField),
                { { meshNodeId }, false, false });
        result.changes.guidesChanged = result.succeeded();
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::createAndAssignGuideCurve(
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().createGuideCurveAndAssignToTrimeshVertexParameter(
                        graph,
                        meshNodeId,
                        vertexIndex,
                        parameterField),
                { { meshNodeId }, false, false });
        result.changes.guidesChanged = result.succeeded();
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::removeGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        const std::vector<String> consumers = guideConsumerNodeIds(graph, guideId);
        auto result = annotateSuccessful(
                GraphEditor().removeGuideCurve(graph, guideId),
                { consumers, false, false });
        result.changes.guidesChanged = result.succeeded() && !consumers.empty();
        result.changes.guidePresentationChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::renameGuideCurve(
        const juce::String& guideId,
        const juce::String& name) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().renameGuideCurve(graph, guideId, name),
                { {}, false, false });
        result.changes.guidePresentationChanged = result.succeeded() && result.changed;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::setGuideHeatmap(
        const juce::String& guideId,
        uint64_t expectedRevision,
        GuideHeatmapAssetPtr asset) {
    return apply([&](auto& graph) {
        const GuideCurveResource* guide = graph.findGuideCurve(guideId);
        if (guide == nullptr) {
            return GraphEditResult { GraphEditCode::MissingNode, guideId, {} };
        }
        if (guide->revision != expectedRevision) {
            return GraphEditResult { GraphEditCode::StaleRevision, guideId, {} };
        }
        const std::vector<String> consumers = guideConsumerNodeIds(graph, guideId);
        auto result = annotateSuccessful(
                GraphEditor().setGuideHeatmap(graph, guideId, std::move(asset)),
                { consumers, false, false });
        result.changes.guidesChanged = result.succeeded() && result.changed && !consumers.empty();
        result.changes.guidePresentationChanged = result.succeeded() && result.changed;
        result.changes.modelChanged = result.succeeded() && result.changed;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::clearGuideHeatmap(
        const juce::String& guideId,
        uint64_t expectedRevision) {
    return apply([&](auto& graph) {
        const GuideCurveResource* guide = graph.findGuideCurve(guideId);
        if (guide == nullptr) {
            return GraphEditResult { GraphEditCode::MissingNode, guideId, {} };
        }
        if (guide->revision != expectedRevision) {
            return GraphEditResult { GraphEditCode::StaleRevision, guideId, {} };
        }
        const std::vector<String> consumers = guideConsumerNodeIds(graph, guideId);
        auto result = annotateSuccessful(
                GraphEditor().clearGuideHeatmap(graph, guideId),
                { consumers, false, false });
        result.changes.guidesChanged = result.succeeded() && result.changed && !consumers.empty();
        result.changes.guidePresentationChanged = result.succeeded() && result.changed;
        result.changes.modelChanged = result.succeeded() && result.changed;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::setNodeParameter(
        const juce::String& nodeId,
        const juce::String& parameterId,
        const juce::String& label,
        const juce::String& value) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeParameter(graph, nodeId, parameterId);
            },
            [&](auto& graph) {
                return GraphEditor().setNodeParameter(
                        graph, nodeId, parameterId, label, value);
            });
}

GraphEditResult GraphCommandDispatcher::replaceNodeModel(
        const juce::String& nodeId,
        uint64_t expectedRevision,
        NodeModelStatePtr model) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeModel(graph, nodeId);
            },
            [&](auto& graph) {
                return GraphEditor().replaceNodeModel(
                        graph, nodeId, expectedRevision, std::move(model));
            });
}

GraphEditResult GraphCommandDispatcher::setNodeEditorState(
        const juce::String& nodeId,
        juce::var editorState) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeEditorState(graph, nodeId);
            },
            [&](auto& graph) {
                return GraphEditor().setNodeEditorState(
                        graph, nodeId, std::move(editorState));
            });
}

GraphEditResult GraphCommandDispatcher::setNodeAudioResource(
        NodeAudioResourceEdit edit) {
    return apply([&](auto& graph) {
        return GraphEditor().setNodeAudioResource(graph, std::move(edit));
    });
}

GraphEditResult GraphCommandDispatcher::removeNodeAudioResource(
        const juce::String& nodeId) {
    return apply([&](auto& graph) {
        return GraphEditor().removeNodeAudioResource(graph, nodeId);
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
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                for (const auto& nodeId : nodeIds) {
                    delta.captureNodeBounds(graph, nodeId);
                }
            },
            [&](auto& graph) {
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
    compoundBefore.reset();
    compoundDelta = {};
    compoundChanges = {};
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
        if (compoundBefore.has_value()) {
            document.recordBeforeChange(std::move(*compoundBefore));
        } else {
            document.recordDelta(compoundDelta.finish(
                    document.graph(), compoundChanges));
        }
        document.publishChange(std::move(compoundChanges));
    }
    compoundBefore.reset();
    compoundDelta = {};
    compoundActive = false;
    compoundChanged = false;
    compoundChanges = {};
    compoundDepth = 0;
}

void GraphCommandDispatcher::cancelCompoundEdit() {
    if (compoundActive && compoundChanged) {
        if (compoundBefore.has_value()) {
            document.restoreGraph(std::move(*compoundBefore));
        } else {
            compoundDelta.restore(document.graphForCommand());
        }
    }
    compoundBefore.reset();
    compoundDelta = {};
    compoundActive = false;
    compoundChanged = false;
    compoundChanges = {};
    compoundDepth = 0;
}

void GraphCommandDispatcher::beginTransientEdit() {
    if (transientEdit.has_value()) {
        ++transientEdit->depth;
        return;
    }
    transientEdit.emplace(TransientEdit {
            NodeGraph::createEditingOverlay(document.graph())
    });
}

void GraphCommandDispatcher::commitTransientEdit() {
    if (!transientEdit.has_value() || --transientEdit->depth > 0) {
        return;
    }
    if (transientEdit->changed) {
        GraphDelta delta = transientEdit->delta.finish(
                transientEdit->graph, transientEdit->changes);
        delta.applyForward(document.graphForCommand());
        document.recordDelta(std::move(delta));
        document.publishChange(std::move(transientEdit->changes));
    }
    transientEdit.reset();
}

void GraphCommandDispatcher::cancelTransientEdit() {
    transientEdit.reset();
}

const NodeGraph& GraphCommandDispatcher::editingGraph() const {
    return transientEdit.has_value() ? transientEdit->graph : document.graph();
}

const GraphChangeSet& GraphCommandDispatcher::transientChanges() const {
    static const GraphChangeSet noChanges;
    return transientEdit.has_value() ? transientEdit->changes : noChanges;
}

GraphEditResult GraphCommandDispatcher::apply(
        const std::function<GraphEditResult(NodeGraph&)>& command) {
    if (transientEdit.has_value()) {
        jassertfalse;
        return { GraphEditCode::ValidationRejected, {}, {} };
    }
    if (compoundActive && !compoundBefore.has_value()) {
        NodeGraph before = document.graph();
        compoundDelta.restore(before);
        compoundBefore.emplace(std::move(before));
    }
    const NodeGraph before = compoundActive ? NodeGraph() : document.graph();
    GraphEditResult result = command(document.graphForCommand());
    if (!result.succeeded()) {
        return result;
    }
    if (!result.changed) {
        return result;
    }

    if (compoundActive) {
        compoundChanged = true;
        accumulateCompoundChange(result.changes);
    } else {
        document.recordBeforeChange(before);
        document.publishChange(result.changes);
    }
    return result;
}

GraphEditResult GraphCommandDispatcher::applyIncremental(
        const std::function<void(GraphDeltaBuilder&, const NodeGraph&)>& capture,
        const std::function<GraphEditResult(NodeGraph&)>& command) {
    if (transientEdit.has_value()) {
        capture(transientEdit->delta, transientEdit->graph);
        GraphEditResult result = command(transientEdit->graph);
        if (result.succeeded() && result.changed) {
            transientEdit->changed = true;
            accumulateChange(transientEdit->changes, result.changes);
        }
        return result;
    }

    if (compoundActive) {
        capture(compoundDelta, document.graph());
        GraphEditResult result = command(document.graphForCommand());
        if (result.succeeded() && result.changed) {
            compoundChanged = true;
            accumulateCompoundChange(result.changes);
        }
        return result;
    }

    GraphDeltaBuilder delta;
    capture(delta, document.graph());
    GraphEditResult result = command(document.graphForCommand());
    if (result.succeeded() && result.changed) {
        document.recordDelta(delta.finish(document.graph(), result.changes));
        document.publishChange(result.changes);
    }
    return result;
}

void GraphCommandDispatcher::accumulateCompoundChange(const GraphChangeSet& change) {
    accumulateChange(compoundChanges, change);
}

void GraphCommandDispatcher::accumulateChange(
        GraphChangeSet& destination,
        const GraphChangeSet& change) {
    for (const auto& nodeId : change.nodeIds) {
        if (std::find(destination.nodeIds.begin(), destination.nodeIds.end(), nodeId)
                == destination.nodeIds.end()) {
            destination.nodeIds.push_back(nodeId);
        }
    }
    destination.removedEdges.insert(
            destination.removedEdges.end(),
            change.removedEdges.begin(),
            change.removedEdges.end());
    destination.removedGuideAssignments.insert(
            destination.removedGuideAssignments.end(),
            change.removedGuideAssignments.begin(),
            change.removedGuideAssignments.end());
    destination.topologyChanged = destination.topologyChanged || change.topologyChanged;
    destination.layoutChanged = destination.layoutChanged || change.layoutChanged;
    destination.probesChanged = destination.probesChanged || change.probesChanged;
    destination.guidesChanged = destination.guidesChanged || change.guidesChanged;
    destination.guidePresentationChanged = destination.guidePresentationChanged
            || change.guidePresentationChanged;
    destination.parameterImpacts = destination.parameterImpacts | change.parameterImpacts;
    destination.modelChanged = destination.modelChanged || change.modelChanged;
    destination.editorStateChanged = destination.editorStateChanged || change.editorStateChanged;
    destination.resourcesChanged = destination.resourcesChanged || change.resourcesChanged;
}

GraphEditResult GraphCommandDispatcher::setNodeBounds(
        const juce::String& nodeId,
        juce::Rectangle<float> bounds) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeBounds(graph, nodeId);
            },
            [&](auto& graph) {
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
