#include <algorithm>
#include <utility>

#include "Runtime/PresentationUpdateRequestBuilder.h"
#include "Runtime/FingerprintBuilder.h"
#include "Nodes/Guide/GuideCurveMeshPreparation.h"

namespace CycleV2 {

uint64_t PresentationUpdateRequestBuilder::effectiveFingerprint(
        const NodeGraph& graph,
        uint64_t documentRevision,
        const GraphChangeSet& change,
        int previewMidiNote,
        int previewModWheelValue) {
    uint64_t effectiveFingerprint = change.nodeIds.empty()
            ? documentRevision
            : 1469598103934665603ULL;
    for (const auto& nodeId : change.nodeIds) {
        const Node* node = graph.findNode(nodeId);
        if (node == nullptr) {
            continue;
        }
        FingerprintBuilder nodeFingerprint(effectiveFingerprint);
        nodeFingerprint.add(nodeId);
        for (const auto& parameter : node->parameters) {
            nodeFingerprint.add(parameter.id).add(parameter.value);
        }
        if (node->model != nullptr) {
            nodeFingerprint.add(node->model->schemaId()).add(node->model->revision());
        }
        if (change.guidesChanged) {
            nodeFingerprint.add(
                    GuideCurveMeshPreparation::configurationKey(graph, nodeId));
        }
        effectiveFingerprint = nodeFingerprint.value();
    }
    effectiveFingerprint = FingerprintBuilder(effectiveFingerprint)
            .add(previewMidiNote)
            .add(previewModWheelValue)
            .value();
    return effectiveFingerprint;
}

CausalUpdateRequest PresentationUpdateRequestBuilder::build(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        const GraphChangeSet& change,
        const EditIdentity& identity,
        const String& sourceStreamId,
        uint64_t effectiveFingerprint,
        bool compile,
        bool preview,
        PresentationRefreshScope scope) {
    std::vector<String> roots = change.nodeIds;
    if (compile || change.probesChanged) {
        for (const auto& probe : graph.getSignalProbes()) {
            if (std::find(roots.begin(), roots.end(), probe.sourceNodeId) == roots.end()) {
                roots.push_back(probe.sourceNodeId);
            }
        }
    }
    if (roots.empty() && !plan.nodeOrder.empty()) {
        roots.push_back(plan.nodeOrder.front());
    }
    std::vector<ProductInvalidation> invalidations;
    const bool probeAddressOnly = change.probesChanged
            && !compile
            && !change.guidesChanged
            && !hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)
            && !hasImpact(change.parameterImpacts, ParameterImpact::Preview)
            && !hasImpact(change.parameterImpacts, ParameterImpact::Presentation);
    for (const auto& root : roots) {
        const std::vector<UpdateCause> causes { { root, compile ? "topology" : "state" } };
        if (identity.phase == EditPhase::Commit) {
            invalidations.push_back({
                    root, sourceStreamId, UpdateProduct::DurablePublication,
                    effectiveFingerprint, causes, false });
        }
        if (scope == PresentationRefreshScope::Downstream
                && (change.guidesChanged
                        || hasImpact(change.parameterImpacts,
                                ParameterImpact::DspConfiguration))) {
            invalidations.push_back({
                    root, sourceStreamId, UpdateProduct::AudioConfiguration,
                    effectiveFingerprint, causes, true });
        }
        if (preview && !probeAddressOnly) {
            invalidations.push_back({
                    root,
                    sourceStreamId,
                    scope == PresentationRefreshScope::LocalEditor
                            ? UpdateProduct::CompactPreview
                            : UpdateProduct::PreviewTraversal,
                    effectiveFingerprint,
                    causes,
                    scope != PresentationRefreshScope::LocalEditor });
        }
        if (preview && scope != PresentationRefreshScope::LocalEditor) {
            invalidations.push_back({
                    root, sourceStreamId, UpdateProduct::ProbePreview,
                    effectiveFingerprint, causes, true });
        }
    }
    std::vector<String> observedNodeIds;
    observedNodeIds.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        if (std::find(observedNodeIds.begin(), observedNodeIds.end(), probe.sourceNodeId)
                == observedNodeIds.end()) {
            observedNodeIds.push_back(probe.sourceNodeId);
        }
    }
    const bool filterToActiveProbes = scope != PresentationRefreshScope::LocalEditor
            && !observedNodeIds.empty();
    return {
            identity,
            std::move(invalidations),
            std::move(observedNodeIds),
            filterToActiveProbes
    };
}

}
