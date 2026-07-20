#include "NodeUpdateGraph.h"

#include <algorithm>
#include <cstdint>

namespace CycleV2 {

namespace {

uint64_t combineFingerprint(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
}

}

bool UpdateCause::operator<(const UpdateCause& other) const {
    if (sourceNodeId != other.sourceNodeId) {
        return sourceNodeId < other.sourceNodeId;
    }
    return field < other.field;
}

UpdateAuditTrace::UpdateAuditTrace(size_t capacityToUse) :
        capacity(capacityToUse) {
}

void UpdateAuditTrace::record(UpdateTraceEvent event) {
    const std::lock_guard<std::mutex> lock(mutex);
    event.sequence = nextSequence++;
    if (capacity == 0) {
        return;
    }
    if (events.size() == capacity) {
        events.pop_front();
    }
    events.push_back(std::move(event));
}

std::vector<UpdateTraceEvent> UpdateAuditTrace::snapshot() const {
    const std::lock_guard<std::mutex> lock(mutex);
    return { events.begin(), events.end() };
}

size_t UpdateAuditTrace::count(
        uint64_t editId,
        const String& nodeId,
        UpdateProduct product,
        UpdateTracePhase phase) const {
    const std::lock_guard<std::mutex> lock(mutex);
    return static_cast<size_t>(std::count_if(events.begin(), events.end(), [&](const auto& event) {
        return event.edit.editId == editId
                && event.nodeId == nodeId
                && event.product == product
                && event.phase == phase;
    }));
}

void UpdateAuditTrace::clear() {
    const std::lock_guard<std::mutex> lock(mutex);
    events.clear();
}

uint64_t SemanticEditGate::beginGesture() {
    activeGestureId = nextGestureId++;
    return activeGestureId;
}

std::optional<EditIdentity> SemanticEditGate::accept(
        const String& sourceStreamId,
        uint64_t effectiveFingerprint,
        EditPhase phase) {
    auto& source = sources[sourceStreamId];
    if (source.initialized && source.effectiveFingerprint == effectiveFingerprint) {
        auditTrace.record({
                {}, sourceStreamId, UpdateProduct::DurablePublication,
                UpdateTracePhase::NoEffectiveChange, {}, effectiveFingerprint, 0 });
        return std::nullopt;
    }

    const bool oneShotCommit = phase == EditPhase::Commit && activeGestureId == 0;
    if (activeGestureId == 0) {
        beginGesture();
    }
    source.effectiveFingerprint = effectiveFingerprint;
    source.initialized = true;
    source.gestureId = activeGestureId;
    EditIdentity identity { nextEditId++, activeGestureId, phase };
    if (phase == EditPhase::Movement) {
        source.lastMovementEditId = identity.editId;
    }
    if (oneShotCommit) {
        source.gestureId = 0;
        activeGestureId = 0;
    }
    return identity;
}

EditIdentity SemanticEditGate::commit(const String& sourceStreamId) {
    auto found = sources.find(sourceStreamId);
    if (found == sources.end() || found->second.gestureId == 0) {
        return {};
    }

    const EditIdentity identity { nextEditId++, found->second.gestureId, EditPhase::Commit };
    found->second.gestureId = 0;
    activeGestureId = 0;
    return identity;
}

void SemanticEditGate::cancelGesture(const String& sourceStreamId) {
    auto found = sources.find(sourceStreamId);
    if (found != sources.end()) {
        found->second.gestureId = 0;
    }
    activeGestureId = 0;
}

bool NodeUpdateGraph::ProductKey::operator<(const ProductKey& other) const {
    if (nodeId != other.nodeId) {
        return nodeId < other.nodeId;
    }
    return product < other.product;
}

bool NodeUpdateGraph::ExecutionKey::operator<(const ExecutionKey& other) const {
    if (editId != other.editId) {
        return editId < other.editId;
    }
    return product < other.product;
}

bool NodeUpdateGraph::GenerationKey::operator<(const GenerationKey& other) const {
    if (sourceStreamId != other.sourceStreamId) {
        return sourceStreamId < other.sourceStreamId;
    }
    return product < other.product;
}

CausalUpdateResult NodeUpdateGraph::execute(
        const GraphExecutionPlan& plan,
        const CausalUpdateRequest& request,
        const ProductExecutor& executor) {
    auto result = executeDeferredPublication(
            plan,
            request,
            [&](const auto& products) {
                return std::all_of(products.begin(), products.end(), [&](const auto& product) {
                    return executor(product);
                });
            });
    publish(request, result);
    return result;
}

CausalUpdateResult NodeUpdateGraph::executeDeferredPublication(
        const GraphExecutionPlan& plan,
        const CausalUpdateRequest& request,
        const ProductBatchExecutor& executor) {
    CausalUpdateResult result;
    if (!request.edit.isValid()) {
        return result;
    }
    std::unique_lock<std::mutex> productLock(productMutex);

    std::map<ProductKey, PlannedNodeProduct> planned;
    std::map<String, std::vector<ProductKey>> productsByNode;
    for (const auto& invalidation : request.invalidations) {
        const auto root = std::find(
                plan.dependencyIndex.nodeIds.begin(),
                plan.dependencyIndex.nodeIds.end(),
                invalidation.sourceNodeId);
        if (root == plan.dependencyIndex.nodeIds.end()) {
            continue;
        }

        const int rootIndex = static_cast<int>(std::distance(
                plan.dependencyIndex.nodeIds.begin(), root));
        std::vector<int> targets { rootIndex };
        if (invalidation.downstream && propagatesDownstream(invalidation.product)) {
            targets = downstreamClosure(plan.dependencyIndex, rootIndex);
        }

        for (const int target : targets) {
            const String& nodeId = plan.dependencyIndex.nodeIds[static_cast<size_t>(target)];
            if (request.filterObservedNodes
                    && invalidation.product == UpdateProduct::ProbePreview
                    && std::find(request.observedNodeIds.begin(), request.observedNodeIds.end(), nodeId)
                            == request.observedNodeIds.end()) {
                PlannedNodeProduct skipped {
                        nodeId, invalidation.product,
                        targetFingerprint(invalidation.inputFingerprint, nodeId, invalidation.product),
                        invalidation.causes };
                trace(request, skipped, UpdateTracePhase::NotObserved);
                continue;
            }

            const ProductKey key { nodeId, invalidation.product };
            if (planned.find(key) == planned.end()) {
                productsByNode[nodeId].push_back(key);
            }
            auto& product = planned[key];
            product.nodeId = nodeId;
            product.product = invalidation.product;
            product.inputFingerprint = combineFingerprint(
                    product.inputFingerprint,
                    targetFingerprint(invalidation.inputFingerprint, nodeId, invalidation.product));
            mergeCauses(product.causes, invalidation.causes);
        }
    }

    for (const auto& nodeId : plan.nodeOrder) {
        const auto nodeProducts = productsByNode.find(nodeId);
        if (nodeProducts == productsByNode.end()) {
            continue;
        }
        for (const auto& productKey : nodeProducts->second) {
            PlannedNodeProduct& product = planned.at(productKey);

            trace(request, product, UpdateTracePhase::Dirtied);
            const ExecutionKey executionKey { request.edit.editId, productKey };
            constexpr size_t maximumLedgerSize = 8192;
            if (executionLedger.size() == maximumLedgerSize) {
                executionLedger.erase(executionLedger.begin());
            }
            if (!executionLedger.insert(executionKey).second) {
                trace(request, product, UpdateTracePhase::InvariantViolation);
                result.invariantViolation = true;
                continue;
            }

            const auto cached = productFingerprints.find(productKey);
            if (cached != productFingerprints.end()
                    && cached->second == product.inputFingerprint) {
                trace(request, product, UpdateTracePhase::AlreadyCurrent);
                continue;
            }

            result.executed.push_back(product);
        }
    }

    productLock.unlock();
    for (const auto& product : result.executed) {
        trace(request, product, UpdateTracePhase::Started);
    }
    if (!result.executed.empty() && !executor(result.executed)) {
        for (const auto& product : result.executed) {
            trace(request, product, UpdateTracePhase::CancelledDuringExecution);
        }
        result.executed.clear();
        return result;
    }
    for (const auto& product : result.executed) {
        trace(request, product, UpdateTracePhase::Completed);
    }

    return result;
}

void NodeUpdateGraph::publish(
        const CausalUpdateRequest& request,
        const CausalUpdateResult& result) {
    const std::lock_guard<std::mutex> lock(productMutex);
    for (const auto& product : result.executed) {
        productFingerprints[{ product.nodeId, product.product }] = product.inputFingerprint;
        trace(request, product, UpdateTracePhase::Published);
    }
}

void NodeUpdateGraph::supersede(
        const String& sourceStreamId,
        UpdateProduct product,
        uint64_t generation) {
    const std::lock_guard<std::mutex> lock(generationMutex);
    auto& current = generations[{ sourceStreamId, product }];
    current = std::max(current, generation);
}

bool NodeUpdateGraph::isCurrent(
        const String& sourceStreamId,
        UpdateProduct product,
        uint64_t generation) const {
    const std::lock_guard<std::mutex> lock(generationMutex);
    const auto found = generations.find({ sourceStreamId, product });
    return found == generations.end() || found->second == generation;
}

void NodeUpdateGraph::clearProductCache() {
    const std::lock_guard<std::mutex> lock(productMutex);
    productFingerprints.clear();
    executionLedger.clear();
}

void NodeUpdateGraph::recordDecision(
        const CausalUpdateRequest& request,
        UpdateTracePhase phase) {
    for (const auto& invalidation : request.invalidations) {
        auditTrace.record({
                request.edit,
                invalidation.sourceNodeId,
                invalidation.product,
                phase,
                invalidation.causes,
                invalidation.inputFingerprint,
                0
        });
    }
}

std::vector<String> NodeUpdateGraph::affectedNodeIds(
        const GraphExecutionPlan& plan,
        const CausalUpdateRequest& request,
        UpdateProduct product) const {
    std::vector<uint8_t> affected(plan.dependencyIndex.nodeIds.size());
    for (const auto& invalidation : request.invalidations) {
        if (invalidation.product != product) {
            continue;
        }
        const auto root = std::find(
                plan.dependencyIndex.nodeIds.begin(),
                plan.dependencyIndex.nodeIds.end(),
                invalidation.sourceNodeId);
        if (root == plan.dependencyIndex.nodeIds.end()) {
            continue;
        }
        const int rootIndex = static_cast<int>(std::distance(
                plan.dependencyIndex.nodeIds.begin(), root));
        const std::vector<int> targets = invalidation.downstream
                ? downstreamClosure(plan.dependencyIndex, rootIndex)
                : std::vector<int> { rootIndex };
        for (const int target : targets) {
            affected[static_cast<size_t>(target)] = 1;
        }
    }

    std::vector<String> result;
    for (size_t index = 0; index < plan.dependencyIndex.nodeIds.size(); ++index) {
        if (affected[index] != 0) {
            result.push_back(plan.dependencyIndex.nodeIds[index]);
        }
    }
    return result;
}

bool NodeUpdateGraph::propagatesDownstream(UpdateProduct product) {
    return product == UpdateProduct::CompactPreview
            || product == UpdateProduct::PreviewTraversal
            || product == UpdateProduct::ProbePreview;
}

uint64_t NodeUpdateGraph::targetFingerprint(
        uint64_t sourceFingerprint,
        const String& nodeId,
        UpdateProduct product) {
    uint64_t fingerprint = combineFingerprint(sourceFingerprint, static_cast<uint64_t>(product));
    return combineFingerprint(fingerprint, static_cast<uint64_t>(nodeId.hashCode64()));
}

std::vector<int> NodeUpdateGraph::downstreamClosure(
        const GraphDependencyIndex& index,
        int sourceIndex) {
    std::vector<uint8_t> visited(index.nodeIds.size());
    std::vector<int> pending { sourceIndex };
    visited[static_cast<size_t>(sourceIndex)] = 1;
    while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        for (const int dependent : index.dependents[static_cast<size_t>(current)]) {
            if (visited[static_cast<size_t>(dependent)] == 0) {
                visited[static_cast<size_t>(dependent)] = 1;
                pending.push_back(dependent);
            }
        }
    }

    std::vector<int> result;
    for (int indexValue = 0; indexValue < static_cast<int>(visited.size()); ++indexValue) {
        if (visited[static_cast<size_t>(indexValue)] != 0) {
            result.push_back(indexValue);
        }
    }
    return result;
}

void NodeUpdateGraph::mergeCauses(
        std::vector<UpdateCause>& destination,
        const std::vector<UpdateCause>& source) {
    destination.insert(destination.end(), source.begin(), source.end());
    std::sort(destination.begin(), destination.end());
    destination.erase(std::unique(destination.begin(), destination.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.sourceNodeId == rhs.sourceNodeId && lhs.field == rhs.field;
    }), destination.end());
}

void NodeUpdateGraph::trace(
        const CausalUpdateRequest& request,
        const PlannedNodeProduct& product,
        UpdateTracePhase phase) {
    auditTrace.record({
            request.edit,
            product.nodeId,
            product.product,
            phase,
            product.causes,
            product.inputFingerprint,
            0
    });
}

}
