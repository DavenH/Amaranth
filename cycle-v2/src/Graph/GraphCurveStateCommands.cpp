#include "Graph/GraphCommandDispatcher.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_set>

#include "Graph/NodeParameterMap.h"

#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

namespace {

bool isCurveNodeKind(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

bool parseGuideControl(const String& value, float& parsed) {
    const char* start = value.toRawUTF8();
    char* end {};
    parsed = std::strtof(start, &end);
    return end != start && *end == '\0' && std::isfinite(parsed);
}

bool guideControlsAreValid(const std::vector<NodeParameter>& controls) {
    std::unordered_set<std::string> remaining {
            "enabled", "noise", "dcOffset", "phase"
    };
    for (const auto& control : controls) {
        if (remaining.erase(control.id.toStdString()) != 1) {
            return false;
        }
        float value {};
        if (!parseGuideControl(control.value, value)) {
            return false;
        }
        if (control.id == "enabled") {
            if (value != 0.f && value != 1.f) {
                return false;
            }
        } else if (value < 0.f || value > 1.f) {
            return false;
        }
    }
    return remaining.empty();
}

bool guideControlsMatch(
        const GuideCurveResource& guide,
        const std::vector<NodeParameter>& controls) {
    if (!guideControlsAreValid(controls)) {
        return false;
    }
    for (const auto& control : controls) {
        float value {};
        parseGuideControl(control.value, value);
        if ((control.id == "enabled" && guide.enabled != (value != 0.f))
                || (control.id == "noise" && guide.noise != value)
                || (control.id == "dcOffset" && guide.dcOffset != value)
                || (control.id == "phase" && guide.phase != value)) {
            return false;
        }
    }
    return true;
}

}

GraphEditResult GraphCommandDispatcher::publishGuideCurveState(
        const GuideCurveStatePublication& publication) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureGuideCurve(graph, publication.guideId);
            },
            [&](NodeGraph& graph) {
                const GuideCurveResource* guide = graph.findGuideCurve(publication.guideId);
                const GuideCurveResource* durableGuide = document.graph().findGuideCurve(
                        publication.guideId);
                if (guide == nullptr || durableGuide == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::MissingNode, publication.guideId, {}
                    };
                }
                if (publication.model == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::InvalidTypedSnapshot,
                            publication.guideId,
                            {}
                    };
                }
                if (!guideControlsAreValid(publication.controls)) {
                    return GraphEditResult {
                            GraphEditCode::InvalidControlValue,
                            publication.guideId,
                            {}
                    };
                }

                const uint64_t currentRevision = guide->revision;
                const uint64_t durableRevision = durableGuide->revision;
                const uint64_t currentModelRevision = guide->model != nullptr
                        ? guide->model->revision()
                        : 0;
                const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
                        publication.model);
                if (typedModel == nullptr || typedModel->flatCurve() == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::InvalidTypedSnapshot,
                            publication.guideId,
                            {}
                    };
                }
                const bool exactRetry = guide->model != nullptr
                        && publication.model->revision() == currentModelRevision
                        && guide->model->equals(*publication.model)
                        && guideControlsMatch(*guide, publication.controls);
                if (exactRetry) {
                    return GraphEditResult {
                            GraphEditCode::Connected,
                            publication.guideId,
                            {},
                            {},
                            false
                    };
                }
                if (!transientEdit.has_value()
                        && publication.model->revision() == currentModelRevision) {
                    return GraphEditResult {
                            GraphEditCode::ConflictingRevision,
                            publication.guideId,
                            {}
                    };
                }
                const bool hasValidTransientBase = transientEdit.has_value()
                        && publication.durableBaseRevision == durableRevision;
                if ((!hasValidTransientBase
                            && publication.durableBaseRevision != currentRevision)
                        || publication.model->revision() < currentModelRevision) {
                    return GraphEditResult {
                            GraphEditCode::StaleRevision, publication.guideId, {}
                    };
                }

                const bool modelChanged = guide->model == nullptr
                        || !guide->model->equals(*publication.model);
                const std::vector<String> consumers = graph.guideTargetNodeIds(
                        publication.guideId);
                GraphEditResult result = GraphEditor().replaceGuideCurve(
                        graph,
                        publication.guideId,
                        publication.model,
                        publication.controls);
                if (result.succeeded()) {
                    result.changes.nodeIds = consumers;
                }
                result.changes.guidesChanged = result.succeeded() && !consumers.empty();
                result.changes.guidePresentationChanged = result.succeeded();
                result.changes.modelChanged = result.succeeded() && modelChanged;
                return result;
            });
}

GraphEditResult GraphCommandDispatcher::publishCurveState(
        const CurveNodeStatePublication& publication) {
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeModel(graph, publication.nodeId);
                delta.captureNodeEditorState(graph, publication.nodeId);
                for (const auto& control : publication.controls) {
                    delta.captureNodeParameter(graph, publication.nodeId, control.id);
                }
            },
            [&](NodeGraph& graph) {
                const Node* node = graph.findNode(publication.nodeId);
                if (node == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::MissingNode, publication.nodeId, {}
                    };
                }
                if (!isCurveNodeKind(node->kind)) {
                    return GraphEditResult {
                            GraphEditCode::WrongNodeKind, publication.nodeId, {}
                    };
                }

                const uint64_t currentRevision = node->model != nullptr
                        ? node->model->revision()
                        : 0;
                if (publication.model == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {}
                    };
                }
                const Node* durableNode = document.graph().findNode(publication.nodeId);
                const NodeModelStatePtr durableModel = durableNode != nullptr
                        ? durableNode->model
                        : NodeModelStatePtr();
                const uint64_t durableRevision = durableModel != nullptr
                        ? durableModel->revision()
                        : 0;
                const bool hasValidTransientBase = transientEdit.has_value()
                        && publication.durableBaseRevision == durableRevision;
                const bool replacesTransientSnapshot = hasValidTransientBase
                        && currentRevision > durableRevision
                        && publication.model->revision() == currentRevision;
                if (!hasValidTransientBase
                        && (publication.durableBaseRevision != currentRevision
                            || publication.model->revision() < currentRevision)) {
                    return GraphEditResult {
                            GraphEditCode::StaleRevision, publication.nodeId, {}
                    };
                }
                if (hasValidTransientBase
                        && publication.model->revision() < currentRevision) {
                    return GraphEditResult {
                            GraphEditCode::StaleRevision, publication.nodeId, {}
                    };
                }
                const auto* definition = NodeDefinitionRegistry::instance().find(node->kind);
                if (definition == nullptr || definition->modelCodec == nullptr
                        || publication.model->schemaId()
                                != definition->modelCodec->schemaId()
                        || publication.model->schemaVersion()
                                != definition->modelCodec->currentVersion()) {
                    return GraphEditResult {
                            GraphEditCode::WrongNodeKind, publication.nodeId, {}
                    };
                }
                const auto typedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
                        publication.model);
                if (typedModel == nullptr) {
                    return GraphEditResult {
                            GraphEditCode::InvalidTypedSnapshot, publication.nodeId, {}
                    };
                }

                std::unordered_set<std::string> requiredControls;
                for (const auto& parameter : definition->parameters) {
                    requiredControls.insert(parameter.id.toStdString());
                }
                std::vector<NodeParameter> parameters;
                parameters.reserve(publication.controls.size());
                for (const auto& control : publication.controls) {
                    const auto required = requiredControls.find(control.id.toStdString());
                    const auto* controlDefinition = NodeDefinitionRegistry::instance()
                            .findParameter(node->kind, control.id);
                    if (required == requiredControls.end() || controlDefinition == nullptr
                            || !controlDefinition->accepts(control.value)) {
                        return GraphEditResult {
                                GraphEditCode::InvalidControlValue,
                                publication.nodeId,
                                {}
                        };
                    }
                    requiredControls.erase(required);
                    parameters.push_back({
                            control.id,
                            controlDefinition->label,
                            controlDefinition->normalized(control.value)
                    });
                }
                if (!requiredControls.empty()) {
                    return GraphEditResult {
                            GraphEditCode::InvalidControlValue, publication.nodeId, {}
                    };
                }
                if (node->kind == NodeKind::Envelope) {
                    const EnvelopeNodeModel* envelope = typedModel->envelope();
                    if (envelope == nullptr) {
                        return GraphEditResult {
                                GraphEditCode::InvalidTypedSnapshot,
                                publication.nodeId,
                                {}
                        };
                    }
                    const NodeParameterMap parameterMap(parameters);
                    if (!parameterMap.contains("red")
                            || !parameterMap.contains("blue")
                            || !parameterMap.contains("logarithmic")
                            || parameterMap.floatValue("red") != envelope->red
                            || parameterMap.floatValue("blue") != envelope->blue
                            || parameterMap.boolValue("logarithmic")
                                    != envelope->logarithmic) {
                        return GraphEditResult {
                                GraphEditCode::InvalidControlValue,
                                publication.nodeId,
                                {}
                        };
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
                    return GraphEditResult {
                            GraphEditCode::ConflictingRevision, publication.nodeId, {}
                    };
                }

                auto parameterResult = GraphEditor().setNodeParametersAtomic(
                        graph, publication.nodeId, parameters);
                if (!parameterResult.succeeded()) {
                    return parameterResult;
                }
                GraphEditResult modelResult;
                if (replacesTransientSnapshot) {
                    modelResult = GraphEditor().replaceTransientNodeModel(
                            graph,
                            publication.nodeId,
                            currentRevision,
                            publication.model);
                } else {
                    modelResult = GraphEditor().replaceNodeModel(
                            graph,
                            publication.nodeId,
                            currentRevision,
                            publication.model);
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
                    modelResult.changes.parameterImpacts =
                            modelResult.changes.parameterImpacts
                            | editorResult.changes.parameterImpacts;
                }
                return modelResult;
            });
}

}
