#include <algorithm>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Nodes/Guide/GuideGraphEditor.h"

namespace CycleV2 {

GraphEditResult GraphCommandDispatcher::addNode(NodeKind kind, juce::Point<float> position) {
    return apply([&](auto& graph) {
        return GraphEditor().addNode(graph, kind, position);
    });
}

GraphEditResult GraphCommandDispatcher::removeNode(const juce::String& nodeId) {
    return apply([&](auto& graph) {
        return GraphEditor().removeNode(graph, nodeId);
    });
}

GraphEditResult GraphCommandDispatcher::removeEdgeAt(size_t edgeIndex) {
    return applyIncremental(
            [&](auto& delta, const auto& graph) {
                if (edgeIndex < graph.getEdges().size()) {
                    const Edge& edge = graph.getEdges()[edgeIndex];
                    delta.captureEdgesToInput(graph, edge.destNodeId, edge.destPortId);
                }
            },
            [&](auto& graph) {
                return GraphEditor().removeEdgeAt(graph, edgeIndex);
            });
}

GraphEditResult GraphCommandDispatcher::connect(const PortAddress& first, const PortAddress& second) {
    const PortAddress& destination = first.input ? first : second;
    return applyIncremental(
            [&](auto& delta, const auto& graph) {
                delta.captureEdgesToInput(graph, destination.nodeId, destination.portId);
            },
            [&](auto& graph) {
                return GraphEditor().connect(graph, first, second);
            });
}

GraphEditResult GraphCommandDispatcher::toggleSignalProbe(
        size_t edgeIndex,
        float tapPosition) {
    return apply([&](auto& graph) {
        return GraphEditor().toggleSignalProbe(graph, edgeIndex, tapPosition);
    });
}

GraphEditResult GraphCommandDispatcher::removeSignalProbe(const juce::String& probeId) {
    return apply([&](auto& graph) {
        return GraphEditor().removeSignalProbe(graph, probeId);
    });
}

GraphEditResult GraphCommandDispatcher::reattachSignalProbe(
        const juce::String& probeId,
        size_t edgeIndex,
        float tapPosition) {
    return apply([&](auto& graph) {
        return GraphEditor().reattachSignalProbe(graph, probeId, edgeIndex, tapPosition);
    });
}

GraphEditResult GraphCommandDispatcher::spliceNodeIntoEdge(
        size_t edgeIndex,
        const juce::String& nodeId) {
    return applyIncremental(
            [&](auto& delta, const auto& graph) {
                if (edgeIndex < graph.getEdges().size()) {
                    const Edge& edge = graph.getEdges()[edgeIndex];
                    delta.captureEdgesToInput(graph, edge.destNodeId, edge.destPortId);
                }
                const Node* node = graph.findNode(nodeId);
                if (node != nullptr) {
                    for (const auto& input : node->inputs) {
                        delta.captureEdgesToInput(graph, nodeId, input.id);
                    }
                }
            },
            [&](auto& graph) {
                return GraphEditor().spliceNodeIntoEdge(graph, edgeIndex, nodeId);
            });
}

GraphEditResult GraphCommandDispatcher::createGuideCurve() {
    return apply([&](auto& graph) {
        return GuideGraphEditor().createGuideCurve(graph);
    });
}

GraphEditResult GraphCommandDispatcher::duplicateGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().duplicateGuideCurve(graph, guideId);
    });
}

GraphEditResult GraphCommandDispatcher::reorderGuideCurve(
        const juce::String& guideId,
        int shelfOrder) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().reorderGuideCurve(graph, guideId, shelfOrder);
    });
}

GraphEditResult GraphCommandDispatcher::assignGuideCurve(
        const juce::String& guideId,
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().assignGuideCurveToMeshComponent(
                graph,
                guideId,
                meshNodeId,
                vertexIndex,
                parameterField);
    });
}

GraphEditResult GraphCommandDispatcher::detachGuideCurve(
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().detachGuideCurveFromMeshComponent(
                graph, meshNodeId, vertexIndex, parameterField);
    });
}

GraphEditResult GraphCommandDispatcher::createAndAssignGuideCurve(
        const juce::String& meshNodeId,
        int vertexIndex,
        const juce::String& parameterField) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().createGuideCurveAndAssignToMeshComponent(
                graph,
                meshNodeId,
                vertexIndex,
                parameterField);
    });
}

GraphEditResult GraphCommandDispatcher::removeGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().removeGuideCurve(graph, guideId);
    });
}

GraphEditResult GraphCommandDispatcher::renameGuideCurve(
        const juce::String& guideId,
        const juce::String& name) {
    return apply([&](auto& graph) {
        return GuideGraphEditor().renameGuideCurve(graph, guideId, name);
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
        return GuideGraphEditor().setGuideHeatmap(graph, guideId, std::move(asset));
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
        return GuideGraphEditor().clearGuideHeatmap(graph, guideId);
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
                return GraphNodeStateEditor().setNodeParameter(
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
                return GraphNodeStateEditor().replaceNodeModel(
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
                return GraphNodeStateEditor().setNodeEditorState(
                        graph, nodeId, std::move(editorState));
            });
}

GraphEditResult GraphCommandDispatcher::setNodeAudioResource(
        NodeAudioResourceEdit edit) {
    return apply([&](auto& graph) {
        return GraphNodeStateEditor().setNodeAudioResource(graph, std::move(edit));
    });
}

GraphEditResult GraphCommandDispatcher::removeNodeAudioResource(
        const juce::String& nodeId) {
    return apply([&](auto& graph) {
        return GraphNodeStateEditor().removeNodeAudioResource(graph, nodeId);
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

bool GraphCommandDispatcher::commitCompoundEdit() {
    if (!compoundActive) {
        return false;
    }
    if (--compoundDepth > 0) {
        return false;
    }
    const bool changed = compoundChanged;
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
    return changed;
}

void GraphCommandDispatcher::cancelCompoundEdit() {
    if (compoundActive && compoundChanged) {
        if (compoundBefore.has_value()) {
            document.graphForCommand() = std::move(*compoundBefore);
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
