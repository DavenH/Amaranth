#include "Runtime/PresentationGestureSession.h"

namespace CycleV2 {

std::optional<EditIdentity> PresentationGestureSession::recordMovement(
        const String& sourceStreamId,
        uint64_t effectiveFingerprint) {
    const auto identity = editGate.accept(
            sourceStreamId,
            effectiveFingerprint,
            EditPhase::Movement);
    if (identity.has_value()) {
        pendingMovement = identity;
        pendingStream = sourceStreamId;
    }
    return identity;
}

std::optional<EditIdentity> PresentationGestureSession::identityForRequest(
        const String& sourceStreamId,
        uint64_t effectiveFingerprint,
        EditPhase phase) {
    if (phase == EditPhase::Commit) {
        const EditIdentity identity = commit(sourceStreamId);
        if (identity.isValid()) {
            return identity;
        }
        return editGate.accept(
                sourceStreamId,
                effectiveFingerprint,
                EditPhase::Commit);
    }
    if (pendingMovement.has_value() && pendingStream == sourceStreamId) {
        const auto identity = pendingMovement;
        pendingMovement.reset();
        return identity;
    }
    return editGate.accept(
            sourceStreamId,
            effectiveFingerprint,
            EditPhase::Movement);
}

EditIdentity PresentationGestureSession::commit(const String& sourceStreamId) {
    const EditIdentity identity = editGate.commit(sourceStreamId);
    pendingMovement.reset();
    pendingStream = {};
    return identity;
}

void PresentationGestureSession::cancel(const String& sourceStreamId) {
    editGate.cancelGesture(sourceStreamId);
    pendingMovement.reset();
    pendingStream = {};
}

String PresentationGestureSession::activeStreamOr(const String& fallback) const {
    return pendingStream.isNotEmpty() ? pendingStream : fallback;
}

}
