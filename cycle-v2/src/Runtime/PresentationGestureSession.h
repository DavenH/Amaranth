#pragma once

#include <optional>
#include <unordered_map>

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
    struct StreamHash {
        size_t operator()(const String& stream) const {
            return static_cast<size_t>(stream.hashCode64());
        }
    };

    SemanticEditGate editGate;
    std::unordered_map<String, EditIdentity, StreamHash> pendingMovements;
    String latestPendingStream;
};

}
