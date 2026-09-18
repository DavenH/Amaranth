#include <vector>

#include "Runtime/PresentationRefreshScheduler.h"

#include "Runtime/FingerprintBuilder.h"

namespace CycleV2 {

CausalUpdateRequest PresentationRefreshScheduler::request(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        uint64_t documentRevision,
        const GraphChangeSet& change,
        PresentationRequestContext context,
        bool compile,
        bool preview,
        PresentationRefreshScope scope) {
    const String stream = gestureSession.activeStreamOr(
            "graph:" + (change.nodeIds.empty() ? String("document") : change.nodeIds.front()));
    const uint64_t fingerprint = PresentationUpdateRequestBuilder::effectiveFingerprint(
            graph,
            documentRevision,
            change,
            context.previewMidiNote,
            context.previewModWheelValue);
    const EditPhase phase = documentRevision > context.publishedGraphRevision
            ? EditPhase::Commit
            : EditPhase::Movement;
    const auto identity = gestureSession.identityForRequest(stream, fingerprint, phase);
    if (!identity.has_value()) {
        return {};
    }
    return PresentationUpdateRequestBuilder::build(
            graph, plan, change, *identity, stream, fingerprint, compile, preview, scope);
}

void PresentationRefreshScheduler::recordEditorMovement(
        NodeUpdateGraph& updateGraph,
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        bool deferredUntilCommit) {
    const String stream = "editor:" + nodeId;
    const uint64_t streamFingerprint = FingerprintBuilder(effectiveFingerprint)
            .add(nodeId)
            .add(field)
            .value();
    const auto identity = gestureSession.graphGestureIsActive(stream)
            ? gestureSession.recordGraphMovement(stream, streamFingerprint)
            : gestureSession.recordMovement(stream, streamFingerprint);
    if (!identity.has_value()) {
        return;
    }
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            plan,
            {
                *identity,
                {
                    {
                        nodeId,
                        stream,
                        UpdateProduct::LocalSlice,
                        streamFingerprint,
                        causes,
                        false
                    }
                },
                {}
            },
            [](const auto&) {
                return true;
            });
    if (deferredUntilCommit) {
        updateGraph.recordDecision(
                {
                    *identity,
                    {
                        {
                            nodeId,
                            stream,
                            UpdateProduct::ProbePreview,
                            streamFingerprint,
                            causes,
                            true
                        }
                    },
                    {}
                },
                UpdateTracePhase::DeferredUntilCommit);
    }
}

bool PresentationRefreshScheduler::commitLocalEditorState(
        NodeUpdateGraph& updateGraph,
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint) {
    const String stream = gestureSession.activeStreamOr("editor:" + nodeId);
    const EditIdentity identity = gestureSession.commit(stream);
    if (!identity.isValid()) {
        return false;
    }
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            plan,
            {
                identity,
                {
                    {
                        nodeId,
                        stream,
                        UpdateProduct::DurablePublication,
                        effectiveFingerprint,
                        causes,
                        false
                    }
                },
                {}
            },
            [](const auto&) {
                return true;
            });
    return true;
}

}
