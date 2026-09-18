#include <algorithm>
#include <vector>

#include "Runtime/PresentationRefreshScheduler.h"

#include "Runtime/FingerprintBuilder.h"

namespace CycleV2 {

CausalUpdateRequest PresentationRefreshScheduler::request(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        uint64_t documentRevision,
        const GraphChangeSet& change,
        PresentationRequestContext context,
        bool compile,
        bool preview,
        PresentationRefreshScope scope) {
    const String stream = gestureSession.activeStreamOr(
            "graph:" + (change.nodeIds.empty() ? String("document") : change.nodeIds.front()));
    const uint64_t fingerprint = PresentationUpdateRequestBuilder::effectiveFingerprint(
            graph,
            documentRevision,
            change,
            context.previewMidiNote,
            context.previewModWheelValue);
    const EditPhase phase = documentRevision > context.publishedGraphRevision
            ? EditPhase::Commit
            : EditPhase::Movement;
    const auto identity = gestureSession.identityForRequest(stream, fingerprint, phase);
    if (!identity.has_value()) {
        return {};
    }
    return PresentationUpdateRequestBuilder::build(
            graph, plan, change, *identity, stream, fingerprint, compile, preview, scope);
}

void PresentationRefreshScheduler::executeSynchronous(
        const GraphExecutionPlan& plan,
        const CausalUpdateRequest& request,
        const NodeUpdateGraph::ProductBatchExecutor& executor) {
    const auto result = updateGraph.executeDeferredPublication(plan, request, executor);
    updateGraph.publish(request, result);
}

void PresentationRefreshScheduler::recordEditorMovement(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        bool deferredUntilCommit) {
    const String stream = "editor:" + nodeId;
    const uint64_t streamFingerprint = FingerprintBuilder(effectiveFingerprint)
            .add(nodeId)
            .add(field)
            .value();
    const auto identity = gestureSession.graphGestureIsActive(stream)
            ? gestureSession.recordGraphMovement(stream, streamFingerprint)
            : gestureSession.recordMovement(stream, streamFingerprint);
    if (!identity.has_value()) {
        return;
    }
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            plan,
            {
                *identity,
                {
                    {
                        nodeId,
                        stream,
                        UpdateProduct::LocalSlice,
                        streamFingerprint,
                        causes,
                        false
                    }
                },
                {}
            },
            [](const auto&) {
                return true;
            });
    if (deferredUntilCommit) {
        updateGraph.recordDecision(
                {
                    *identity,
                    {
                        {
                            nodeId,
                            stream,
                            UpdateProduct::ProbePreview,
                            streamFingerprint,
                            causes,
                            true
                        }
                    },
                    {}
                },
                UpdateTracePhase::DeferredUntilCommit);
    }
}

bool PresentationRefreshScheduler::commitLocalEditorState(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint) {
    const String stream = gestureSession.activeStreamOr("editor:" + nodeId);
    const EditIdentity identity = gestureSession.commit(stream);
    if (!identity.isValid()) {
        return false;
    }
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            plan,
            {
                identity,
                {
                    {
                        nodeId,
                        stream,
                        UpdateProduct::DurablePublication,
                        effectiveFingerprint,
                        causes,
                        false
                    }
                },
                {}
            },
            [](const auto&) {
                return true;
            });
    return true;
}

PresentationRefreshScheduler::~PresentationRefreshScheduler() {
    shutdown();
}

uint64_t PresentationRefreshScheduler::beginAsyncRequest() {
    return currentGeneration.fetch_add(1) + 1;
}

void PresentationRefreshScheduler::enqueue(
        uint64_t generation,
        AsyncRefresh refresh,
        GraphPresentationPerformanceMetrics& performance,
        ExecuteProducts executeProducts,
        AcceptPublication acceptPublication) {
    refresh.generation = generation;
    for (const auto& invalidation : refresh.request.invalidations) {
        updateGraph.supersede(
                invalidation.sourceStreamId,
                invalidation.product,
                generation);
    }
    auto job = std::make_shared<AsyncRefresh>(std::move(refresh));
    asyncWorker.post(
            [this, job, &performance,
                    executeProducts = std::move(executeProducts)] {
                return executeAsyncRefresh(
                        *job, performance, executeProducts);
            },
            [this, job, &performance,
                    acceptPublication = std::move(acceptPublication)] {
                publishAsyncRefresh(
                        *job, performance, acceptPublication);
            });
}

bool PresentationRefreshScheduler::executeAsyncRefresh(
        AsyncRefresh& refresh,
        GraphPresentationPerformanceMetrics& performance,
        const ExecuteProducts& executeProducts) {
    using Performance = GraphPresentationPerformanceMetrics;
    const uint64_t startedAt = performance.timestamp();
    performance.record(
            Performance::Stage::QueueDelay,
            startedAt - refresh.requestedAtMicroseconds);
    if (!isCurrent(refresh)) {
        updateGraph.recordDecision(
                refresh.request, UpdateTracePhase::SupersededBeforeStart);
        performance.record(Performance::Outcome::SupersededBeforeStart);
        return false;
    }
    refresh.updateResult = updateGraph.executeDeferredPublication(
            refresh.snapshot.compileResult.plan,
            refresh.request,
            [&](const auto& products) {
                return executeProducts(refresh, products);
            });
    refresh.workerFinishedAtMicroseconds = performance.timestamp();
    performance.record(
            Performance::Stage::Worker,
            refresh.workerFinishedAtMicroseconds - startedAt);
    const bool prepared = isCurrent(refresh);
    if (!prepared) {
        performance.record(Performance::Outcome::StaleOrCancelled);
    }
    return prepared;
}

void PresentationRefreshScheduler::publishAsyncRefresh(
        AsyncRefresh& refresh,
        GraphPresentationPerformanceMetrics& performance,
        const AcceptPublication& acceptPublication) {
    using Performance = GraphPresentationPerformanceMetrics;
    const uint64_t startedAt = performance.timestamp();
    if (refresh.workerFinishedAtMicroseconds != 0) {
        performance.record(
                Performance::Stage::PublicationDelay,
                startedAt - refresh.workerFinishedAtMicroseconds);
    }
    if (!alive.load()) {
        performance.record(Performance::Outcome::StaleOrCancelled);
        return;
    }
    if (!isCurrent(refresh)
            || refresh.generation < publishedGeneration
            || !acceptPublication(refresh)) {
        updateGraph.recordDecision(
                refresh.request, UpdateTracePhase::StaleResultDiscarded);
        performance.record(Performance::Outcome::StaleOrCancelled);
        return;
    }
    publishedGeneration = refresh.generation;
    updateGraph.publish(refresh.request, refresh.updateResult);
    performance.record(
            Performance::Stage::EndToEnd,
            performance.timestamp() - refresh.requestedAtMicroseconds);
    performance.record(Performance::Outcome::Published);
    if (refresh.completion) {
        refresh.completion();
    }
}

bool PresentationRefreshScheduler::isCurrent(
        const AsyncRefresh& refresh) const {
    if (!alive.load() || refresh.generation != currentGeneration.load()) {
        return false;
    }
    return std::all_of(
            refresh.request.invalidations.begin(),
            refresh.request.invalidations.end(),
            [&](const auto& invalidation) {
                return updateGraph.isCurrent(
                        invalidation.sourceStreamId,
                        invalidation.product,
                        refresh.generation);
            });
}

void PresentationRefreshScheduler::cancelAndWait() {
    currentGeneration.fetch_add(1);
    asyncWorker.cancelAndWait();
}

void PresentationRefreshScheduler::shutdown() {
    alive.store(false);
    currentGeneration.fetch_add(1);
    asyncWorker.shutdown();
}

}
