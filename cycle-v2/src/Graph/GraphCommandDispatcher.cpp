#include "GraphCommandDispatcher.h"

#include "NodeParameterMap.h"

#include "../Nodes/Effect2D/CurveNodeModels.h"

#include <algorithm>
#include <unordered_set>

namespace CycleV2 {

namespace {

bool isCurveNodeKind(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

struct EditAnnotation {
    std::vector<juce::String> nodeIds;
    bool topologyChanged {};
    bool layoutChanged {};
};

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
        result.changes.guidesChanged = result.succeeded();
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::duplicateGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().duplicateGuideCurve(graph, guideId),
                { {}, false, false });
        result.changes.guidesChanged = result.succeeded();
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
        result.changes.guidesChanged = result.succeeded() && result.changed;
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
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::removeGuideCurve(const juce::String& guideId) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().removeGuideCurve(graph, guideId),
                { {}, false, false });
        result.changes.guidesChanged = result.succeeded();
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
        result.changes.guidesChanged = result.succeeded() && result.changed;
        return result;
    });
}

GraphEditResult GraphCommandDispatcher::replaceGuideCurve(
        const juce::String& guideId,
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls) {
    return apply([&](auto& graph) {
        auto result = annotateSuccessful(
                GraphEditor().replaceGuideCurve(graph, guideId, std::move(model), controls),
                { {}, false, false });
        result.changes.guidesChanged = result.succeeded();
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

GraphEditResult GraphCommandDispatcher::replaceNodeModel(
        const juce::String& nodeId,
        uint64_t expectedRevision,
        NodeModelStatePtr model) {
    return apply([&](auto& graph) {
        return GraphEditor().replaceNodeModel(graph, nodeId, expectedRevision, std::move(model));
    });
}

GraphEditResult GraphCommandDispatcher::setNodeEditorState(
        const juce::String& nodeId,
        juce::var editorState) {
    return apply([&](auto& graph) {
        return GraphEditor().setNodeEditorState(graph, nodeId, std::move(editorState));
    });
}

GraphEditResult GraphCommandDispatcher::publishCurveState(
        const CurveNodeStatePublication& publication) {
    return apply([&](auto& graph) {
        const Node* node = graph.findNode(publication.nodeId);
        if (node == nullptr) {
            return GraphEditResult { GraphEditCode::MissingNode, publication.nodeId, {} };
        }
        if (!isCurveNodeKind(node->kind)) {
            return GraphEditResult { GraphEditCode::WrongNodeKind, publication.nodeId, {} };
        }

        const uint64_t currentRevision = node->model != nullptr ? node->model->revision() : 0;
        if (publication.model == nullptr) {
            return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
        }
        const Node* durableNode = document.graph().findNode(publication.nodeId);
        const uint64_t durableRevision = durableNode != nullptr && durableNode->model != nullptr
                ? durableNode->model->revision()
                : 0;
        const bool hasValidTransientBase = transientEdit.has_value()
                && publication.durableBaseRevision == durableRevision;
        const bool replacesTransientSnapshot = hasValidTransientBase
                && currentRevision > durableRevision
                && publication.model->revision() == currentRevision;
        if (!hasValidTransientBase
                && (publication.durableBaseRevision != currentRevision
                    || publication.model->revision() < currentRevision)) {
            return GraphEditResult { GraphEditCode::StaleRevision, publication.nodeId, {} };
        }
        if (hasValidTransientBase && publication.model->revision() < currentRevision) {
            return GraphEditResult { GraphEditCode::StaleRevision, publication.nodeId, {} };
        }
        const auto* definition = NodeDefinitionRegistry::instance().find(node->kind);
        if (definition == nullptr || definition->modelCodec == nullptr
                || publication.model->schemaId() != definition->modelCodec->schemaId()
                || publication.model->schemaVersion() != definition->modelCodec->currentVersion()) {
            return GraphEditResult { GraphEditCode::WrongNodeKind, publication.nodeId, {} };
        }
        const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(publication.model);
        if (typedModel == nullptr) {
            return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
        }

        std::unordered_set<std::string> requiredControls;
        for (const auto& parameter : definition->parameters) {
            requiredControls.insert(parameter.id.toStdString());
        }
        std::vector<NodeParameter> parameters;
        parameters.reserve(publication.controls.size());
        for (const auto& control : publication.controls) {
            const auto required = requiredControls.find(control.id.toStdString());
            const auto* controlDefinition = NodeDefinitionRegistry::instance().findParameter(
                    node->kind, control.id);
            if (required == requiredControls.end() || controlDefinition == nullptr
                    || !controlDefinition->accepts(control.value)) {
                return GraphEditResult { GraphEditCode::InvalidControlValue, publication.nodeId, {} };
            }
            requiredControls.erase(required);
            parameters.push_back({ control.id, controlDefinition->label, controlDefinition->normalized(control.value) });
        }
        if (!requiredControls.empty()) {
            return GraphEditResult { GraphEditCode::InvalidControlValue, publication.nodeId, {} };
        }
        if (node->kind == NodeKind::Envelope) {
            const EnvelopeNodeModel* envelope = typedModel->envelope();
            if (envelope == nullptr) {
                return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
            }
            const NodeParameterMap parameterMap(parameters);
            if (!parameterMap.contains("red")
                    || !parameterMap.contains("blue")
                    || !parameterMap.contains("logarithmic")
                    || parameterMap.floatValue("red") != envelope->red
                    || parameterMap.floatValue("blue") != envelope->blue
                    || parameterMap.boolValue("logarithmic") != envelope->logarithmic) {
                return GraphEditResult { GraphEditCode::InvalidControlValue, publication.nodeId, {} };
            }
        }
        const bool isModelRetry = node->model != nullptr
                && node->model->revision() == publication.model->revision()
                && node->model->equals(*publication.model);
        const bool controlsMatch = node->parameters.size() == parameters.size()
                && std::equal(
                        node->parameters.begin(),
                        node->parameters.end(),
                        parameters.begin(),
                        [](const NodeParameter& left, const NodeParameter& right) {
                            return left.id == right.id && left.value == right.value;
                        });
        if (isModelRetry && !controlsMatch && !transientEdit.has_value()) {
            return GraphEditResult { GraphEditCode::ConflictingRevision, publication.nodeId, {} };
        }

        auto parameterResult = GraphEditor().setNodeParametersAtomic(graph, publication.nodeId, parameters);
        if (!parameterResult.succeeded()) {
            return parameterResult;
        }
        GraphEditResult modelResult;
        if (replacesTransientSnapshot) {
            modelResult = GraphEditor().replaceTransientNodeModel(
                    graph, publication.nodeId, currentRevision, publication.model);
        } else {
            modelResult = GraphEditor().replaceNodeModel(
                    graph, publication.nodeId, currentRevision, publication.model);
        }
        modelResult.changed = modelResult.changed || parameterResult.changed;
        accumulateChange(modelResult.changes, parameterResult.changes);
        if (typedModel->editorJSON().getDynamicObject() != nullptr) {
            auto editorResult = GraphEditor().setNodeEditorState(
                    graph, publication.nodeId, typedModel->editorJSON());
            if (!editorResult.succeeded()) {
                return editorResult;
            }
            modelResult.changed = modelResult.changed || editorResult.changed;
            modelResult.changes.editorStateChanged = editorResult.changed;
            modelResult.changes.parameterImpacts = modelResult.changes.parameterImpacts
                    | editorResult.changes.parameterImpacts;
        }
        return modelResult;
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

GraphEditResult GraphCommandDispatcher::setPerformanceKeyboardBounds(
        juce::Rectangle<float> bounds) {
    return apply([&](auto& graph) {
        GraphEditResult result;
        result.changed = graph.setPerformanceKeyboardBounds(bounds);
        result.changes.layoutChanged = result.changed;
        return result;
    });
}

void GraphCommandDispatcher::beginCompoundEdit() {
    if (compoundActive) {
        ++compoundDepth;
        return;
    }
    compoundBefore = document.toJson();
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
        document.recordBeforeChange(std::move(compoundBefore));
        document.publishChange(std::move(compoundChanges));
    }
    compoundBefore = {};
    compoundActive = false;
    compoundChanged = false;
    compoundChanges = {};
    compoundDepth = 0;
}

void GraphCommandDispatcher::cancelCompoundEdit() {
    if (compoundActive && compoundChanged) {
        document.restoreJson(compoundBefore);
    }
    compoundBefore = {};
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
    transientEdit = TransientEdit { document.graph() };
}

void GraphCommandDispatcher::commitTransientEdit() {
    if (!transientEdit.has_value() || --transientEdit->depth > 0) {
        return;
    }
    if (transientEdit->changed) {
        document.recordBeforeChange(document.toJson());
        document.currentGraph = std::move(transientEdit->graph);
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
        GraphEditResult result = command(transientEdit->graph);
        if (result.succeeded() && result.changed) {
            transientEdit->changed = true;
            accumulateChange(transientEdit->changes, result.changes);
        }
        return result;
    }
    const juce::String before = compoundActive ? juce::String() : document.toJson();
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
    destination.topologyChanged = destination.topologyChanged || change.topologyChanged;
    destination.layoutChanged = destination.layoutChanged || change.layoutChanged;
    destination.probesChanged = destination.probesChanged || change.probesChanged;
    destination.guidesChanged = destination.guidesChanged || change.guidesChanged;
    destination.parameterImpacts = destination.parameterImpacts | change.parameterImpacts;
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
