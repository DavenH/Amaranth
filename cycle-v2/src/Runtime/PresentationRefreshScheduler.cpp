#include "Runtime/PresentationRefreshScheduler.h"

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

}
