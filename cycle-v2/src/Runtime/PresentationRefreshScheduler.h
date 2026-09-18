#pragma once

#include "Runtime/PresentationGestureSession.h"
#include "Runtime/PresentationUpdateRequestBuilder.h"

namespace CycleV2 {

struct PresentationRequestContext {
    uint64_t publishedGraphRevision {};
    int previewMidiNote { 48 };
    int previewModWheelValue {};
};

class PresentationRefreshScheduler final {
public:
    CausalUpdateRequest request(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            uint64_t documentRevision,
            const GraphChangeSet& change,
            PresentationRequestContext context,
            bool compile,
            bool preview,
            PresentationRefreshScope scope);
    void recordEditorMovement(
            NodeUpdateGraph& updateGraph,
            const GraphExecutionPlan& plan,
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint,
            bool deferredUntilCommit);
    bool commitLocalEditorState(
            NodeUpdateGraph& updateGraph,
            const GraphExecutionPlan& plan,
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint);

    PresentationGestureSession& editSession() { return gestureSession; }

private:
    PresentationGestureSession gestureSession;
};

}
