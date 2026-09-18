#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Runtime/GraphPresentationPerformanceMetrics.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/MessageThreadWorker.h"
#include "Runtime/PresentationGestureSession.h"
#include "Runtime/PresentationUpdateRequestBuilder.h"

namespace CycleV2 {

struct PresentationRequestContext {
    uint64_t publishedGraphRevision {};
    int previewMidiNote { 48 };
    int previewModWheelValue {};
};

class PresentationRefreshScheduler final {
public:
    struct AsyncRefresh {
        uint64_t generation {};
        std::shared_ptr<const NodeGraph> graph;
        GraphChangeSet change;
        PresentationRefreshScope scope { PresentationRefreshScope::Downstream };
        CausalUpdateRequest request;
        CausalUpdateResult updateResult;
        GraphPresentationSnapshot snapshot;
        std::function<void()> completion;
        uint64_t requestedAtMicroseconds {};
        uint64_t workerFinishedAtMicroseconds {};
        bool previewRendered {};
    };

    using ExecuteProducts = std::function<bool(
            AsyncRefresh&, const std::vector<PlannedNodeProduct>&)>;
    using AcceptPublication = std::function<bool(AsyncRefresh&)>;

    ~PresentationRefreshScheduler();

    CausalUpdateRequest request(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            uint64_t documentRevision,
            const GraphChangeSet& change,
            PresentationRequestContext context,
            bool compile,
            bool preview,
            PresentationRefreshScope scope);
    void executeSynchronous(
            const GraphExecutionPlan& plan,
            const CausalUpdateRequest& request,
            const NodeUpdateGraph::ProductBatchExecutor& executor);
    void clearProductCache() { updateGraph.clearProductCache(); }
    const UpdateAuditTrace& trace() const { return updateGraph.trace(); }
    void recordEditorMovement(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint,
            bool deferredUntilCommit);
    bool commitLocalEditorState(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint);
    uint64_t beginAsyncRequest();
    void enqueueLocalProduct(
            GraphPresentationPerformanceMetrics& performance,
            std::function<void()> render,
            std::function<void()> publish);
    void enqueue(
            uint64_t generation,
            AsyncRefresh refresh,
            GraphPresentationPerformanceMetrics& performance,
            ExecuteProducts executeProducts,
            AcceptPublication acceptPublication);
    bool isCurrent(const AsyncRefresh& refresh) const;
    void cancelAndWait();
    void shutdown();

    PresentationGestureSession& editSession() { return gestureSession; }

private:
    bool executeAsyncRefresh(
            AsyncRefresh& refresh,
            GraphPresentationPerformanceMetrics& performance,
            const ExecuteProducts& executeProducts);
    void publishAsyncRefresh(
            AsyncRefresh& refresh,
            GraphPresentationPerformanceMetrics& performance,
            const AcceptPublication& acceptPublication);

    PresentationGestureSession gestureSession;
    NodeUpdateGraph updateGraph;
    MessageThreadWorker asyncWorker;
    std::atomic<bool> alive { true };
    std::atomic<uint64_t> currentGeneration {};
    uint64_t publishedGeneration {};
};

}
