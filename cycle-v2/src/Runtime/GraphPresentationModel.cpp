#include "GraphPresentationModel.h"
#include "FingerprintBuilder.h"

#include <algorithm>

namespace CycleV2 {

namespace {

}

GraphPresentationModel::GraphPresentationModel() :
        asyncState(std::make_shared<AsyncState>()) {
}

GraphPresentationModel::~GraphPresentationModel() {
    asyncState->alive.store(false);
    asyncState->generation.fetch_add(1);
    asyncWorker.shutdown();
}

bool GraphPresentationModel::refresh(
        const NodeGraph& graph,
        uint64_t documentRevision,
        const GraphChangeSet& change) {
    requestedGraphRevision = documentRevision;
    const bool compile = current.graphRevision == 0 || requiresCompilation(change);
    const bool preview = compile || requiresPreview(change);
    if (compile) {
        asyncState->generation.fetch_add(1);
        asyncWorker.cancelAndWait();
    }

    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    if (compile) {
        next.compileResult = compiler.compile(graph);
        next.runtimeTrace = {};
        ++compilations;
        if (next.compileResult.succeeded()) {
            next.runtimeTrace = GraphRuntime().process(graph, next.compileResult.plan);
        }
        updateGraph.clearProductCache();
        previewAudioExecutor.clearIncrementalCache();
    } else if (hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)) {
        refreshConfigurations(graph, next.compileResult.plan, change.nodeIds);
    }

    bool previewRendered {};
    const auto request = updateRequest(
            graph,
            next.compileResult.plan,
            documentRevision,
            change,
            compile,
            preview);
    if (!compile && !request.edit.isValid()) {
        return true;
    }
    updateGraph.execute(next.compileResult.plan, request, [&](const auto& product) {
        if (product.product != UpdateProduct::PreviewTraversal
                && product.product != UpdateProduct::ProbePreview
                && product.product != UpdateProduct::CompactPreview) {
            return true;
        }
        if (previewRendered) {
            return true;
        }
        previewRendered = true;
        next.previewResult = {};
        if (next.compileResult.succeeded()) {
            constexpr size_t previewFrameCount = 128;
            const AudioExecutionSpec spec {
                    previewFrameCount,
                    44100.0,
                    ChannelLayout::LinkedStereo
            };
            previewAudioExecutor.prepareExecution(next.compileResult.plan, spec);
            const GraphAudioResult audio = previewAudioExecutor.process(
                    graph,
                    next.compileResult.plan,
                    previewFrameCount);
            next.previewResult = GraphPreviewExecutor().render(
                    next.compileResult.plan,
                    audio,
                    graph.getSignalProbes(),
                    40);
            ++previewRenders;
        }
        return true;
    });

    return acceptSnapshot(std::move(next));
}

bool GraphPresentationModel::acceptSnapshot(GraphPresentationSnapshot snapshotToAccept) {
    if (snapshotToAccept.graphRevision != requestedGraphRevision
            || snapshotToAccept.graphRevision < current.graphRevision) {
        return false;
    }

    current = std::move(snapshotToAccept);
    ++presentationRevision;
    return true;
}

void GraphPresentationModel::refreshAsync(
        NodeGraph graph,
        uint64_t documentRevision,
        GraphChangeSet change,
        std::function<void()> completion) {
    if (current.graphRevision == 0 || requiresCompilation(change)) {
        refresh(graph, documentRevision, change);
        if (completion) {
            completion();
        }
        return;
    }

    requestedGraphRevision = documentRevision;
    const uint64_t generation = asyncState->generation.fetch_add(1) + 1;
    const bool preview = requiresPreview(change);
    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    const auto request = updateRequest(
            graph,
            next.compileResult.plan,
            documentRevision,
            change,
            false,
            preview);
    if (!request.edit.isValid() || request.invalidations.empty()) {
        acceptSnapshot(std::move(next));
        if (completion) {
            completion();
        }
        return;
    }
    for (const auto& invalidation : request.invalidations) {
        updateGraph.supersede(
                invalidation.sourceStreamId,
                invalidation.product,
                generation);
    }
    const uint64_t requestFingerprint = request.invalidations.empty()
            ? 0
            : request.invalidations.front().inputFingerprint;
    if (request.edit.phase == EditPhase::Commit
            && requestFingerprint != 0
            && requestFingerprint == publishedEditFingerprint) {
        updateGraph.execute(next.compileResult.plan, request, [](const auto&) {
            return true;
        });
        acceptSnapshot(std::move(next));
        if (completion) {
            completion();
        }
        return;
    }
    auto refresh = std::make_shared<AsyncRefresh>();
    refresh->state = asyncState;
    refresh->generation = generation;
    refresh->graph = std::move(graph);
    refresh->change = std::move(change);
    refresh->request = request;
    refresh->requestFingerprint = requestFingerprint;
    refresh->snapshot = std::move(next);
    refresh->completion = std::move(completion);
    asyncWorker.post([this, refresh] {
        if (!isCurrent(*refresh)) {
            updateGraph.recordDecision(
                    refresh->request, UpdateTracePhase::SupersededBeforeStart);
            return false;
        }
        return prepareAsyncRefresh(*refresh);
    }, [this, refresh] {
        auto completion = publishAsyncRefresh(refresh);
        if (completion) {
            completion();
        }
    });
}

bool GraphPresentationModel::prepareAsyncRefresh(AsyncRefresh& refresh) {
    refresh.updateResult = updateGraph.executeDeferredPublication(
            refresh.snapshot.compileResult.plan,
            refresh.request,
            [&](const auto& products) {
                return executeAsyncProducts(refresh, products);
            });
    return isCurrent(refresh);
}

bool GraphPresentationModel::executeAsyncProducts(
        AsyncRefresh& refresh,
        const std::vector<PlannedNodeProduct>& products) {
    auto& next = refresh.snapshot;
    const bool preparesConfiguration = std::any_of(
            products.begin(), products.end(), [](const auto& product) {
                return product.product == UpdateProduct::AudioConfiguration;
            });
    if (preparesConfiguration) {
        refreshConfigurations(refresh.graph, next.compileResult.plan, refresh.change.nodeIds);
    }
    if (!isCurrent(refresh) || !requiresPreview(refresh.change)
            || !next.compileResult.succeeded()) {
        return isCurrent(refresh);
    }

    constexpr size_t previewFrameCount = 128;
    const AudioExecutionSpec spec {
            previewFrameCount,
            44100.0,
            ChannelLayout::LinkedStereo
    };
    previewAudioExecutor.prepareExecution(next.compileResult.plan, spec);
    std::vector<uint8_t> dirtyNodes(next.compileResult.plan.steps.size());
    for (const auto& product : products) {
        if (product.product == UpdateProduct::PreviewTraversal
                && product.nodeIndex >= 0
                && static_cast<size_t>(product.nodeIndex) < dirtyNodes.size()) {
            dirtyNodes[static_cast<size_t>(product.nodeIndex)] = 1;
        }
    }
    const GraphAudioResultView audio = previewAudioExecutor.processIncrementalIndexed(
            refresh.graph,
            next.compileResult.plan,
            previewFrameCount,
            dirtyNodes,
            [&] { return isCurrent(refresh); });
    if (audio.cancelled || !isCurrent(refresh)) {
        return false;
    }
    GraphPreviewExecutor().renderIncremental(
            next.compileResult.plan,
            audio,
            refresh.graph.getSignalProbes(),
            dirtyNodes,
            40,
            next.previewResult);
    refresh.previewRendered = true;
    return true;
}

std::function<void()> GraphPresentationModel::publishAsyncRefresh(
        std::shared_ptr<AsyncRefresh> refresh) {
    if (!refresh->state->alive.load()) {
        return {};
    }
    if (!isCurrent(*refresh)) {
        updateGraph.recordDecision(refresh->request, UpdateTracePhase::StaleResultDiscarded);
        return {};
    }
    if (refresh->generation < publishedGeneration
            || !acceptSnapshot(std::move(refresh->snapshot))) {
        updateGraph.recordDecision(refresh->request, UpdateTracePhase::StaleResultDiscarded);
        return {};
    }
    publishedGeneration = refresh->generation;
    updateGraph.publish(refresh->request, refresh->updateResult);
    if (refresh->previewRendered) {
        ++previewRenders;
    }
    publishedEditFingerprint = refresh->requestFingerprint;
    return std::move(refresh->completion);
}

bool GraphPresentationModel::isCurrent(const AsyncRefresh& refresh) const {
    if (!refresh.state->alive.load()) {
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

void GraphPresentationModel::recordEditorMovement(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        bool deferredUntilCommit) {
    const String stream = "editor:" + nodeId;
    const uint64_t streamFingerprint = FingerprintBuilder(effectiveFingerprint)
            .add(nodeId)
            .add(field)
            .value();
    const auto identity = editGate.accept(stream, streamFingerprint, EditPhase::Movement);
    if (!identity.has_value()) {
        return;
    }
    latestMovementIdentity = identity;
    latestMovementStream = stream;
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            current.compileResult.plan,
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

void GraphPresentationModel::commitLocalEditorState(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        uint64_t documentRevision) {
    const String stream = latestMovementStream.isNotEmpty()
            ? latestMovementStream
            : "editor:" + nodeId;
    const EditIdentity identity = editGate.commit(stream);
    latestMovementIdentity.reset();
    latestMovementStream = {};
    if (!identity.isValid()) {
        return;
    }
    const std::vector<UpdateCause> causes { { nodeId, field } };
    updateGraph.execute(
            current.compileResult.plan,
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
    requestedGraphRevision = documentRevision;
    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    acceptSnapshot(std::move(next));
}

GraphAudioResult GraphPresentationModel::captureAudio(
        const NodeGraph& graph,
        size_t frameCount) const {
    if (!current.compileResult.succeeded()) {
        return {};
    }

    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    GraphAudioExecutor captureExecutor;
    captureExecutor.prepareExecution(current.compileResult.plan, spec);
    return captureExecutor.process(graph, current.compileResult.plan, frameCount);
}

bool GraphPresentationModel::requiresCompilation(const GraphChangeSet& change) const {
    return change.topologyChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::GraphSemantics);
}

bool GraphPresentationModel::requiresPreview(const GraphChangeSet& change) const {
    return change.probesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)
            || hasImpact(change.parameterImpacts, ParameterImpact::Preview)
            || hasImpact(change.parameterImpacts, ParameterImpact::Presentation);
}

void GraphPresentationModel::refreshConfigurations(
        const NodeGraph& graph,
        GraphExecutionPlan& plan,
        const std::vector<String>& nodeIds) {
    AudioExecutionSpec spec;
    for (auto& step : plan.steps) {
        if (!nodeIds.empty()
                && std::find(nodeIds.begin(), nodeIds.end(), step.nodeId) == nodeIds.end()) {
            continue;
        }
        const Node* node = graph.findNode(step.nodeId);
        if (node == nullptr) {
            continue;
        }
        step.parameters = node->parameters;
        const String key = configurationFactory.keyFor(step.audioRole, step.parameters, spec);
        if (step.configuration.key == key) {
            continue;
        }
        auto value = configurationFactory.create(step.audioRole, step.parameters, spec);
        if (value != nullptr) {
            step.configuration = {
                    step.configuration.revision + 1,
                    key,
                    std::move(value)
            };
        }
    }
}

CausalUpdateRequest GraphPresentationModel::updateRequest(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        uint64_t documentRevision,
        const GraphChangeSet& change,
        bool compile,
        bool preview) {
    const String stream = latestMovementStream.isNotEmpty()
            ? latestMovementStream
            : "graph:" + (change.nodeIds.empty() ? String("document") : change.nodeIds.front());
    uint64_t effectiveFingerprint = change.nodeIds.empty()
            ? documentRevision
            : 1469598103934665603ULL;
    for (const auto& nodeId : change.nodeIds) {
        const Node* node = graph.findNode(nodeId);
        if (node == nullptr) {
            continue;
        }
        FingerprintBuilder nodeFingerprint(effectiveFingerprint);
        nodeFingerprint.add(nodeId);
        for (const auto& parameter : node->parameters) {
            nodeFingerprint.add(parameter.id).add(parameter.value);
        }
        effectiveFingerprint = nodeFingerprint.value();
    }
    const EditPhase phase = documentRevision > current.graphRevision
            ? EditPhase::Commit
            : EditPhase::Movement;
    std::optional<EditIdentity> identity;
    if (phase == EditPhase::Commit) {
        const EditIdentity committed = editGate.commit(stream);
        latestMovementIdentity.reset();
        latestMovementStream = {};
        if (committed.isValid()) {
            identity = committed;
        } else {
            identity = editGate.accept(stream, effectiveFingerprint, EditPhase::Commit);
        }
    } else if (latestMovementIdentity.has_value()) {
        identity = latestMovementIdentity;
        latestMovementIdentity.reset();
    } else {
        identity = editGate.accept(stream, effectiveFingerprint, EditPhase::Movement);
    }
    if (!identity.has_value()) {
        return {};
    }

    std::vector<String> roots = change.nodeIds;
    if (roots.empty() && !plan.nodeOrder.empty()) {
        roots.push_back(plan.nodeOrder.front());
    }
    std::vector<ProductInvalidation> invalidations;
    for (const auto& root : roots) {
        const std::vector<UpdateCause> causes { { root, compile ? "topology" : "state" } };
        if (hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)) {
            invalidations.push_back({
                    root, stream, UpdateProduct::AudioConfiguration,
                    effectiveFingerprint, causes, true });
        }
        if (preview) {
            invalidations.push_back({
                    root, stream, UpdateProduct::PreviewTraversal,
                    effectiveFingerprint, causes, true });
            invalidations.push_back({
                    root, stream, UpdateProduct::ProbePreview,
                    effectiveFingerprint, causes, true });
        }
    }
    std::vector<String> observedNodeIds;
    observedNodeIds.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        if (std::find(observedNodeIds.begin(), observedNodeIds.end(), probe.sourceNodeId)
                == observedNodeIds.end()) {
            observedNodeIds.push_back(probe.sourceNodeId);
        }
    }
    const bool filterToActiveProbes = !observedNodeIds.empty();
    return {
            *identity,
            std::move(invalidations),
            std::move(observedNodeIds),
            filterToActiveProbes
    };
}

}
