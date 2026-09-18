#pragma once

#include <optional>

#include "Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

class PresentationGestureSession final {
public:
    std::optional<EditIdentity> recordMovement(
            const String& sourceStreamId,
            uint64_t effectiveFingerprint);
    std::optional<EditIdentity> identityForRequest(
            const String& sourceStreamId,
            uint64_t effectiveFingerprint,
            EditPhase phase);
    EditIdentity commit(const String& sourceStreamId);
    void cancel(const String& sourceStreamId);
    String activeStreamOr(const String& fallback) const;

private:
    SemanticEditGate editGate;
    std::optional<EditIdentity> pendingMovement;
    String pendingStream;
};

}
