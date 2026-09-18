#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "Graph/GraphCommandDispatcher.h"
#include "Runtime/NodeUpdateGraph.h"
#include "Runtime/PresentationRefreshPolicy.h"

namespace CycleV2 {

class PresentationGestureSession final {
public:
    struct GraphGestureFinish {
        std::shared_ptr<const NodeGraph> finalSnapshot;
        bool live {};
        bool changed {};
        bool durableChanged {};
    };

    bool beginGraphGesture(
            const String& sourceStreamId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document,
            ProbeRefreshMode mode,
            uint64_t initialFingerprint);
    bool graphGestureIsActive(const String& sourceStreamId) const;
    bool graphGestureIsLive(const String& sourceStreamId) const;
    std::optional<EditIdentity> recordGraphMovement(
            const String& sourceStreamId,
            uint64_t effectiveFingerprint);
    std::shared_ptr<const NodeGraph> snapshotGraphGesture(
            const String& sourceStreamId,
            const GraphCommandDispatcher& commands);
    GraphGestureFinish finishGraphGesture(
            const String& sourceStreamId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document);
    void cancelGraphGesture(
            const String& sourceStreamId,
            GraphCommandDispatcher& commands);

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

    struct GraphGestureState {
        std::shared_ptr<const NodeGraph> stableGraph;
        std::shared_ptr<const NodeGraph> latestSnapshot;
        uint64_t baseRevision {};
        uint64_t effectiveFingerprint {};
        bool live {};
        bool changed {};
    };

    SemanticEditGate editGate;
    std::unordered_map<String, GraphGestureState, StreamHash> graphGestures;
    std::unordered_map<String, EditIdentity, StreamHash> pendingMovements;
    String latestPendingStream;
};

}
