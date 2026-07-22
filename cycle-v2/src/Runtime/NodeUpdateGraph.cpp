#include "NodeUpdateGraph.h"
#include "FingerprintBuilder.h"

#include <algorithm>
#include <cstdint>

namespace CycleV2 {

namespace {

bool isObservedProductTarget(
        UpdateProduct product,
        int target,
        const std::vector<uint8_t>& observed,
        const std::vector<uint8_t>& leadsToObservation) {
    if (product == UpdateProduct::ProbePreview) {
        return observed[static_cast<size_t>(target)] != 0;
    }
    if (product == UpdateProduct::PreviewTraversal) {
        return leadsToObservation[static_cast<size_t>(target)] != 0;
    }
    return true;
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

    for (auto& affected : result.affected) {
        affected.assign(plan.dependencyIndex.nodeIds.size(), 0);
    }

    planningSlots.resize(plan.dependencyIndex.nodeIds.size());
    productFingerprints.resize(plan.dependencyIndex.nodeIds.size());
    lastExecutedEdit.resize(plan.dependencyIndex.nodeIds.size());
    for (auto& nodeProducts : planningSlots) {
        for (auto& product : nodeProducts) {
            product.reset();
        }
    }
    prepareObservationMask(plan.dependencyIndex, request.observedNodeIds);
    for (const auto& invalidation : request.invalidations) {
        const auto root = plan.dependencyIndex.nodeIndexById.find(invalidation.sourceNodeId);
        if (root == plan.dependencyIndex.nodeIndexById.end()) {
            continue;
        }
        const int rootIndex = root->second;
        traversalTargets.clear();
        traversalTargets.push_back(rootIndex);
        if (invalidation.downstream && propagatesDownstream(invalidation.product)) {
            downstreamClosure(plan.dependencyIndex, rootIndex);
        }

        for (const int target : traversalTargets) {
            const String& nodeId = plan.dependencyIndex.nodeIds[static_cast<size_t>(target)];
            if (request.filterObservedNodes
                    && !isObservedProductTarget(
                            invalidation.product,
                            target,
                            observedNodes,
                            leadsToObservation)) {
                PlannedNodeProduct skipped {
                        nodeId, invalidation.product,
                        targetFingerprint(invalidation.inputFingerprint, nodeId, invalidation.product),
                        invalidation.causes,
                        target };
                trace(request, skipped, UpdateTracePhase::NotObserved);
                continue;
            }

            result.affected[static_cast<size_t>(invalidation.product)]
                    [static_cast<size_t>(target)] = 1;

            auto& productSlot = planningSlots[static_cast<size_t>(target)]
                    [static_cast<size_t>(invalidation.product)];
            if (!productSlot.has_value()) {
                productSlot = PlannedNodeProduct {
                        nodeId, invalidation.product, 0, {}, target };
            }
            auto& product = *productSlot;
            product.nodeId = nodeId;
            product.product = invalidation.product;
            product.inputFingerprint = combineFingerprint(
                    product.inputFingerprint,
                    targetFingerprint(invalidation.inputFingerprint, nodeId, invalidation.product));
            mergeCauses(product.causes, invalidation.causes);
        }
    }

    for (size_t nodeIndex = 0; nodeIndex < planningSlots.size(); ++nodeIndex) {
        for (auto& productSlot : planningSlots[nodeIndex]) {
            if (!productSlot.has_value()) {
                continue;
            }
            PlannedNodeProduct& product = *productSlot;
            const size_t productIndex = static_cast<size_t>(product.product);

            trace(request, product, UpdateTracePhase::Dirtied);
            uint64_t& editStamp = lastExecutedEdit[nodeIndex][productIndex];
            if (editStamp == request.edit.editId) {
                trace(request, product, UpdateTracePhase::InvariantViolation);
                result.invariantViolation = true;
                continue;
            }
            editStamp = request.edit.editId;

            const auto& cached = productFingerprints[nodeIndex][productIndex];
            if (cached.has_value() && *cached == product.inputFingerprint) {
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
        if (product.nodeIndex >= 0
                && static_cast<size_t>(product.nodeIndex) < productFingerprints.size()) {
            productFingerprints[static_cast<size_t>(product.nodeIndex)]
                    [static_cast<size_t>(product.product)] = product.inputFingerprint;
        }
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
    lastExecutedEdit.clear();
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
    std::vector<uint8_t> observed(plan.dependencyIndex.nodeIds.size());
    std::vector<uint8_t> leadsToObserved(plan.dependencyIndex.nodeIds.size());
    std::vector<int> observationPending;
    for (const auto& nodeId : request.observedNodeIds) {
        const auto found = plan.dependencyIndex.nodeIndexById.find(nodeId);
        if (found != plan.dependencyIndex.nodeIndexById.end()) {
            observed[static_cast<size_t>(found->second)] = 1;
            leadsToObserved[static_cast<size_t>(found->second)] = 1;
            observationPending.push_back(found->second);
        }
    }
    while (!observationPending.empty()) {
        const int current = observationPending.back();
        observationPending.pop_back();
        for (const int dependency : plan.dependencyIndex.dependencies[static_cast<size_t>(current)]) {
            if (leadsToObserved[static_cast<size_t>(dependency)] == 0) {
                leadsToObserved[static_cast<size_t>(dependency)] = 1;
                observationPending.push_back(dependency);
            }
        }
    }
    for (const auto& invalidation : request.invalidations) {
        if (invalidation.product != product) {
            continue;
        }
        const auto root = plan.dependencyIndex.nodeIndexById.find(invalidation.sourceNodeId);
        if (root == plan.dependencyIndex.nodeIndexById.end()) {
            continue;
        }
        const int rootIndex = root->second;
        std::vector<uint8_t> targets(plan.dependencyIndex.nodeIds.size());
        targets[static_cast<size_t>(rootIndex)] = 1;
        std::vector<int> pending { rootIndex };
        while (invalidation.downstream && !pending.empty()) {
            const int current = pending.back();
            pending.pop_back();
            for (const int dependent : plan.dependencyIndex.dependents[static_cast<size_t>(current)]) {
                if (targets[static_cast<size_t>(dependent)] == 0) {
                    targets[static_cast<size_t>(dependent)] = 1;
                    pending.push_back(dependent);
                }
            }
        }
        for (int target = 0; target < static_cast<int>(targets.size()); ++target) {
            if (targets[static_cast<size_t>(target)] == 0) {
                continue;
            }
            if (!request.filterObservedNodes
                    || isObservedProductTarget(product, target, observed, leadsToObserved)) {
                affected[static_cast<size_t>(target)] = 1;
            }
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

void NodeUpdateGraph::prepareObservationMask(
        const GraphDependencyIndex& index,
        const std::vector<String>& observedNodeIds) {
    observedNodes.assign(index.nodeIds.size(), 0);
    leadsToObservation.assign(index.nodeIds.size(), 0);
    traversalPending.clear();
    for (const auto& nodeId : observedNodeIds) {
        const auto found = index.nodeIndexById.find(nodeId);
        if (found == index.nodeIndexById.end()) {
            continue;
        }
        observedNodes[static_cast<size_t>(found->second)] = 1;
        leadsToObservation[static_cast<size_t>(found->second)] = 1;
        traversalPending.push_back(found->second);
    }
    while (!traversalPending.empty()) {
        const int current = traversalPending.back();
        traversalPending.pop_back();
        for (const int dependency : index.dependencies[static_cast<size_t>(current)]) {
            if (leadsToObservation[static_cast<size_t>(dependency)] == 0) {
                leadsToObservation[static_cast<size_t>(dependency)] = 1;
                traversalPending.push_back(dependency);
            }
        }
    }
}

const std::vector<int>& NodeUpdateGraph::downstreamClosure(
        const GraphDependencyIndex& index,
        int sourceIndex) {
    if (closureVisitGeneration.size() != index.nodeIds.size()) {
        closureVisitGeneration.assign(index.nodeIds.size(), 0);
    }
    if (++currentClosureGeneration == 0) {
        closureVisitGeneration.assign(index.nodeIds.size(), 0);
        ++currentClosureGeneration;
    }
    traversalPending.clear();
    traversalTargets.clear();
    traversalPending.push_back(sourceIndex);
    closureVisitGeneration[static_cast<size_t>(sourceIndex)] = currentClosureGeneration;
    while (!traversalPending.empty()) {
        const int current = traversalPending.back();
        traversalPending.pop_back();
        for (const int dependent : index.dependents[static_cast<size_t>(current)]) {
            if (closureVisitGeneration[static_cast<size_t>(dependent)]
                    != currentClosureGeneration) {
                closureVisitGeneration[static_cast<size_t>(dependent)] = currentClosureGeneration;
                traversalPending.push_back(dependent);
            }
        }
    }

    for (int nodeIndex = 0; nodeIndex < static_cast<int>(index.nodeIds.size()); ++nodeIndex) {
        if (closureVisitGeneration[static_cast<size_t>(nodeIndex)] == currentClosureGeneration) {
            traversalTargets.push_back(nodeIndex);
        }
    }
    return traversalTargets;
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
