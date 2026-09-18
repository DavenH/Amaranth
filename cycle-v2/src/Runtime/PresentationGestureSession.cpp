#include "Runtime/PresentationGestureSession.h"

namespace CycleV2 {

bool PresentationGestureSession::beginGraphGesture(
        const String& sourceStreamId,
        GraphCommandDispatcher& commands,
        const GraphDocument& document,
        ProbeRefreshMode mode,
        uint64_t initialFingerprint) {
    if (graphGestureIsActive(sourceStreamId)) {
        return false;
    }

    GraphGestureState state;
    state.baseRevision = document.revision();
    state.effectiveFingerprint = initialFingerprint;
    state.live = PresentationRefreshPolicy::schedulesDownstreamDuringMovement(mode);
    if (state.live) {
        if (commands.hasTransientEdit()) {
            return false;
        }
        state.stableGraph = std::make_shared<const NodeGraph>(commands.editingGraph());
        commands.beginTransientEdit();
    }
    graphGestures.emplace(sourceStreamId, std::move(state));
    return true;
}

bool PresentationGestureSession::graphGestureIsActive(
        const String& sourceStreamId) const {
    return graphGestures.find(sourceStreamId) != graphGestures.end();
}

bool PresentationGestureSession::graphGestureIsLive(
        const String& sourceStreamId) const {
    const auto found = graphGestures.find(sourceStreamId);
    return found != graphGestures.end() && found->second.live;
}

std::optional<EditIdentity> PresentationGestureSession::recordGraphMovement(
        const String& sourceStreamId,
        uint64_t effectiveFingerprint) {
    const auto found = graphGestures.find(sourceStreamId);
    if (found == graphGestures.end()
            || found->second.effectiveFingerprint == effectiveFingerprint) {
        return std::nullopt;
    }
    const auto identity = recordMovement(sourceStreamId, effectiveFingerprint);
    if (identity.has_value()) {
        found->second.effectiveFingerprint = effectiveFingerprint;
        found->second.changed = true;
    }
    return identity;
}

std::shared_ptr<const NodeGraph> PresentationGestureSession::snapshotGraphGesture(
        const String& sourceStreamId,
        const GraphCommandDispatcher& commands) {
    const auto found = graphGestures.find(sourceStreamId);
    if (found == graphGestures.end() || !found->second.live) {
        return {};
    }
    auto snapshot = std::make_shared<const NodeGraph>(
            commands.editingGraph().snapshotNodeEdits(found->second.stableGraph));
    found->second.latestSnapshot = snapshot;
    return snapshot;
}

PresentationGestureSession::GraphGestureFinish
PresentationGestureSession::finishGraphGesture(
        const String& sourceStreamId,
        GraphCommandDispatcher& commands,
        const GraphDocument& document) {
    const auto found = graphGestures.find(sourceStreamId);
    if (found == graphGestures.end()) {
        return {};
    }
    GraphGestureState state = std::move(found->second);
    graphGestures.erase(found);
    if (state.live) {
        commands.commitTransientEdit();
    }
    const bool durableChanged = document.revision() != state.baseRevision;
    if (!state.changed || (state.live && !durableChanged)) {
        commit(sourceStreamId);
    }
    return {
            std::move(state.latestSnapshot),
            state.live,
            state.changed,
            durableChanged
    };
}

void PresentationGestureSession::cancelGraphGesture(
        const String& sourceStreamId,
        GraphCommandDispatcher& commands) {
    const auto found = graphGestures.find(sourceStreamId);
    if (found != graphGestures.end() && found->second.live) {
        commands.cancelTransientEdit();
    }
    graphGestures.erase(sourceStreamId);
    cancel(sourceStreamId);
}

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
