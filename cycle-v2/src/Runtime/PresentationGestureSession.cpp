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
        pendingMovements.insert_or_assign(sourceStreamId, *identity);
        latestPendingStream = sourceStreamId;
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
    const auto pending = pendingMovements.find(sourceStreamId);
    if (pending != pendingMovements.end()) {
        const auto identity = pending->second;
        pendingMovements.erase(pending);
        return identity;
    }
    return editGate.accept(
            sourceStreamId,
            effectiveFingerprint,
            EditPhase::Movement);
}

EditIdentity PresentationGestureSession::commit(const String& sourceStreamId) {
    const EditIdentity identity = editGate.commit(sourceStreamId);
    pendingMovements.erase(sourceStreamId);
    if (latestPendingStream == sourceStreamId) {
        latestPendingStream = pendingMovements.empty()
                ? String()
                : pendingMovements.begin()->first;
    }
    return identity;
}

void PresentationGestureSession::cancel(const String& sourceStreamId) {
    editGate.cancelGesture(sourceStreamId);
    pendingMovements.erase(sourceStreamId);
    if (latestPendingStream == sourceStreamId) {
        latestPendingStream = pendingMovements.empty()
                ? String()
                : pendingMovements.begin()->first;
    }
}

String PresentationGestureSession::activeStreamOr(const String& fallback) const {
    return latestPendingStream.isNotEmpty() ? latestPendingStream : fallback;
}

}
