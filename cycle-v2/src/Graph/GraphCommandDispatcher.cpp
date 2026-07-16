#include "GraphCommandDispatcher.h"

#include "../Nodes/Effect2D/CurveNodeModels.h"

#include <algorithm>
#include <unordered_set>

namespace CycleV2 {

namespace {

bool isCurveNodeKind(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::GuideCurve
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

bool canonicalCurveSnapshot(
        NodeKind kind,
        const juce::String& snapshot,
        uint64_t& revision,
        juce::String& canonical) {
    if (kind == NodeKind::Envelope) {
        EnvelopeNodeModel model;
        if (!model.loadSnapshot(snapshot)) {
            return false;
        }
        revision = model.revision();
        canonical = model.snapshot();
        return true;
    }

    FlatCurveModel model;
    if (!model.loadSnapshot(snapshot)) {
        return false;
    }
    revision = model.revision();
    canonical = model.snapshot();
    return true;
}

const NodeParameter* findParameter(const std::vector<NodeParameter>& parameters, const juce::String& id) {
    const auto found = std::find_if(parameters.begin(), parameters.end(), [&](const auto& parameter) {
        return parameter.id == id;
    });
    return found != parameters.end() ? &*found : nullptr;
}

}

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

        const uint64_t currentRevision = CurveNodeModelCodec::revisionFromParameters(node->parameters);
        if (publication.expectedRevision != currentRevision || publication.revision < currentRevision) {
            return GraphEditResult { GraphEditCode::StaleRevision, publication.nodeId, {} };
        }

        uint64_t snapshotRevision {};
        juce::String canonicalSnapshot;
        if (!canonicalCurveSnapshot(
                    node->kind, publication.modelSnapshot, snapshotRevision, canonicalSnapshot)
                || snapshotRevision != publication.revision) {
            return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
        }
        uint64_t currentSnapshotRevision {};
        juce::String canonicalCurrentSnapshot;
        if (!canonicalCurveSnapshot(
                    node->kind,
                    CurveNodeModelCodec::snapshotFromParameters(node->parameters),
                    currentSnapshotRevision,
                    canonicalCurrentSnapshot)
                || currentSnapshotRevision != currentRevision) {
            return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
        }

        const auto* definition = NodeDefinitionRegistry::instance().find(node->kind);
        if (definition == nullptr) {
            return GraphEditResult { GraphEditCode::WrongNodeKind, publication.nodeId, {} };
        }

        std::unordered_set<std::string> requiredControls;
        for (const auto& parameter : definition->parameters) {
            if (parameter.id != CurveNodeModelCodec::snapshotParameterId()
                    && parameter.id != CurveNodeModelCodec::revisionParameterId()) {
                requiredControls.insert(parameter.id.toStdString());
            }
        }
        std::vector<NodeParameter> parameters;
        parameters.reserve(publication.controls.size() + 2);
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
            EnvelopeNodeModel envelope;
            if (!envelope.loadSnapshot(canonicalSnapshot)) {
                return GraphEditResult { GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {} };
            }
            const auto* red = findParameter(parameters, "red");
            const auto* blue = findParameter(parameters, "blue");
            const auto* logarithmic = findParameter(parameters, "logarithmic");
            if (red == nullptr || blue == nullptr || logarithmic == nullptr
                    || red->value.getFloatValue() != envelope.red
                    || blue->value.getFloatValue() != envelope.blue
                    || (logarithmic->value.getIntValue() != 0) != envelope.logarithmic) {
                return GraphEditResult { GraphEditCode::InvalidControlValue, publication.nodeId, {} };
            }
        }
        parameters.push_back({
                CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot", canonicalSnapshot });
        parameters.push_back({
                CurveNodeModelCodec::revisionParameterId(),
                "Curve Model Revision",
                juce::String((int64_t) publication.revision) });

        if (publication.revision == currentRevision) {
            const bool identical = std::all_of(parameters.begin(), parameters.end(), [&](const auto& parameter) {
                const auto* current = findParameter(node->parameters, parameter.id);
                if (current == nullptr) {
                    return false;
                }
                if (parameter.id == CurveNodeModelCodec::snapshotParameterId()) {
                    return canonicalCurrentSnapshot == parameter.value;
                }
                return current->value == parameter.value;
            });
            if (!identical) {
                return GraphEditResult { GraphEditCode::ConflictingRevision, publication.nodeId, {} };
            }
        }

        return GraphEditor().setNodeParametersAtomic(graph, publication.nodeId, parameters);
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
    if (!result.changed) {
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
