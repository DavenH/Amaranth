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

    PresentationGestureSession& editSession() { return gestureSession; }

private:
    PresentationGestureSession gestureSession;
};

}
