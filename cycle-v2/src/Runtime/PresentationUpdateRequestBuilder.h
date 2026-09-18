#pragma once

#include <cstdint>

#include "Graph/GraphEditTypes.h"
#include "Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

enum class PresentationRefreshScope {
    Downstream,
    LocalEditor,
    PreviewOnly
};

class PresentationUpdateRequestBuilder final {
public:
    static uint64_t effectiveFingerprint(
            const NodeGraph& graph,
            uint64_t documentRevision,
            const GraphChangeSet& change,
            int previewMidiNote,
            int previewModWheelValue);
    static CausalUpdateRequest build(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            const GraphChangeSet& change,
            const EditIdentity& identity,
            const String& sourceStreamId,
            uint64_t effectiveFingerprint,
            bool compile,
            bool preview,
            PresentationRefreshScope scope);
};

}
