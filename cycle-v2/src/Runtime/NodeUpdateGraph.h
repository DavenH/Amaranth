#pragma once

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "../Graph/GraphCompiler.h"

namespace CycleV2 {

enum class EditPhase {
    Movement,
    Commit
};

struct EditIdentity {
    uint64_t editId {};
    uint64_t gestureId {};
    EditPhase phase { EditPhase::Movement };

    bool isValid() const { return editId != 0; }
};

enum class UpdateProduct {
    LocalSlice,
    LocalSurface,
    InteractionOverlay,
    CompactPreview,
    PreviewTraversal,
    ProbePreview,
    AudioConfiguration,
    DurablePublication
};

constexpr size_t updateProductCount =
        static_cast<size_t>(UpdateProduct::DurablePublication) + 1;

enum class UpdateTracePhase {
    Dirtied,
    Started,
    Completed,
    Published,
    AlreadyCurrent,
    NotObserved,
    DeferredUntilCommit,
    SupersededBeforeStart,
    CancelledDuringExecution,
    StaleResultDiscarded,
    NoEffectiveChange,
    InvariantViolation
};

enum class ProbeRefreshMode {
    OnGestureCommit,
    LiveLatest
};

struct UpdateCause {
    String sourceNodeId;
    String field;

    bool operator<(const UpdateCause& other) const;
};

struct UpdateTraceEvent {
    EditIdentity edit;
    String nodeId;
    UpdateProduct product {};
    UpdateTracePhase phase {};
    std::vector<UpdateCause> causes;
    uint64_t inputFingerprint {};
    uint64_t sequence {};
};

struct ProductInvalidation {
    String sourceNodeId;
    String sourceStreamId;
    UpdateProduct product {};
    uint64_t inputFingerprint {};
    std::vector<UpdateCause> causes;
    bool downstream {};
};

struct CausalUpdateRequest {
    EditIdentity edit;
    std::vector<ProductInvalidation> invalidations;
    std::vector<String> observedNodeIds;
    bool filterObservedNodes {};
};

struct PlannedNodeProduct {
    String nodeId;
    UpdateProduct product {};
    uint64_t inputFingerprint {};
    std::vector<UpdateCause> causes;
    int nodeIndex { -1 };
};

struct CausalUpdateResult {
    std::vector<PlannedNodeProduct> executed;
    std::array<std::vector<uint8_t>, updateProductCount> affected;
    bool invariantViolation {};
};

class UpdateAuditTrace {
public:
    explicit UpdateAuditTrace(size_t capacityToUse = 2048);

    void record(UpdateTraceEvent event);
    std::vector<UpdateTraceEvent> snapshot() const;
    size_t count(
            uint64_t editId,
            const String& nodeId,
            UpdateProduct product,
            UpdateTracePhase phase) const;
    void clear();

private:
    mutable std::mutex mutex;
    std::deque<UpdateTraceEvent> events;
    size_t capacity;
    uint64_t nextSequence { 1 };
};

class SemanticEditGate {
public:
    uint64_t beginGesture();
    std::optional<EditIdentity> accept(
            const String& sourceStreamId,
            uint64_t effectiveFingerprint,
            EditPhase phase = EditPhase::Movement);
    EditIdentity commit(const String& sourceStreamId);
    void cancelGesture(const String& sourceStreamId);

    const UpdateAuditTrace& trace() const { return auditTrace; }

private:
    struct SourceState {
        uint64_t effectiveFingerprint {};
        uint64_t gestureId {};
        uint64_t lastMovementEditId {};
        bool initialized {};
    };

    uint64_t nextEditId { 1 };
    uint64_t nextGestureId { 1 };
    uint64_t activeGestureId {};
    std::map<String, SourceState> sources;
    UpdateAuditTrace auditTrace;
};

class NodeUpdateGraph {
public:
    using ProductExecutor = std::function<bool(const PlannedNodeProduct&)>;
    using ProductBatchExecutor = std::function<bool(const std::vector<PlannedNodeProduct>&)>;

    CausalUpdateResult execute(
            const GraphExecutionPlan& plan,
            const CausalUpdateRequest& request,
            const ProductExecutor& executor);
    CausalUpdateResult executeDeferredPublication(
            const GraphExecutionPlan& plan,
            const CausalUpdateRequest& request,
            const ProductBatchExecutor& executor);
    void publish(
            const CausalUpdateRequest& request,
            const CausalUpdateResult& result);
    void supersede(
            const String& sourceStreamId,
            UpdateProduct product,
            uint64_t generation);
    bool isCurrent(
            const String& sourceStreamId,
            UpdateProduct product,
            uint64_t generation) const;
    void clearProductCache();
    void recordDecision(
            const CausalUpdateRequest& request,
            UpdateTracePhase phase);
    std::vector<String> affectedNodeIds(
            const GraphExecutionPlan& plan,
            const CausalUpdateRequest& request,
            UpdateProduct product) const;

    const UpdateAuditTrace& trace() const { return auditTrace; }

private:
    static constexpr size_t productCount = updateProductCount;
    using PlanningSlots = std::array<std::optional<PlannedNodeProduct>, productCount>;
    using FingerprintSlots = std::array<std::optional<uint64_t>, productCount>;
    using EditStampSlots = std::array<uint64_t, productCount>;

    struct GenerationKey {
        String sourceStreamId;
        UpdateProduct product {};

        bool operator<(const GenerationKey& other) const;
    };

    static bool propagatesDownstream(UpdateProduct product);
    static uint64_t targetFingerprint(
            uint64_t sourceFingerprint,
            const String& nodeId,
            UpdateProduct product);
    void prepareObservationMask(
            const GraphDependencyIndex& index,
            const std::vector<String>& observedNodeIds);
    const std::vector<int>& downstreamClosure(
            const GraphDependencyIndex& index,
            int sourceIndex);
    static void mergeCauses(
            std::vector<UpdateCause>& destination,
            const std::vector<UpdateCause>& source);
    void trace(
            const CausalUpdateRequest& request,
            const PlannedNodeProduct& product,
            UpdateTracePhase phase);
    std::vector<FingerprintSlots> productFingerprints;
    std::vector<EditStampSlots> lastExecutedEdit;
    std::vector<PlanningSlots> planningSlots;
    std::vector<uint8_t> observedNodes;
    std::vector<uint8_t> leadsToObservation;
    std::vector<int> traversalPending;
    std::vector<int> traversalTargets;
    std::vector<uint64_t> closureVisitGeneration;
    uint64_t currentClosureGeneration {};
    mutable std::mutex productMutex;
    mutable std::mutex generationMutex;
    std::map<GenerationKey, uint64_t> generations;
    UpdateAuditTrace auditTrace;
};

}
