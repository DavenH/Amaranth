#include "GraphPresentationModel.h"
#include "FingerprintBuilder.h"

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <optional>
#include <set>

namespace CycleV2 {

namespace {

const SignalProbe* findProbe(const NodeGraph& graph, const String& probeId) {
    const auto found = std::find_if(
            graph.getSignalProbes().begin(),
            graph.getSignalProbes().end(),
            [&](const auto& candidate) {
                return candidate.id == probeId;
            });
    return found == graph.getSignalProbes().end() ? nullptr : &*found;
}

std::optional<int> attachedKeyNote(
        const NodeGraph& graph,
        const String& voiceContextId,
        int fallbackMidiNote) {
    const Range<int> midiRange {
            Constants::LowestMidiNote,
            Constants::HighestMidiNote
    };
    for (const auto& edge : graph.getEdges()) {
        if (edge.destNodeId != voiceContextId
                || edge.attachmentType != AttachmentType::ModulationTriple) {
            continue;
        }
        const Node* triple = graph.findNode(edge.sourceNodeId);
        if (triple == nullptr || triple->kind != NodeKind::ModulationTriple) {
            continue;
        }

        const float fallbackKey = Arithmetic::getUnitValueForGraphicNote(
                fallbackMidiNote,
                midiRange);
        const float key = typedParameterFloat(
                triple->parameters,
                "redConstant",
                fallbackKey);
        return Arithmetic::getGraphicNoteForValue(key, midiRange);
    }
    return std::nullopt;
}

GraphPreviewResult captureProbePreviews(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        int midiNote) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = 44100.0;
    GraphAudioExecutor captureExecutor;
    captureExecutor.prepareExecution(plan, spec);

    AudioVoiceContext voice;
    voice.controls.noteNumber = jlimit(0, 127, midiNote);
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const GraphAudioResult audio = captureExecutor.process(
            graph,
            plan,
            frameCount,
            {},
            voice);
    return GraphPreviewExecutor().render(
            plan,
            audio,
            graph.getSignalProbes(),
            frameCount);
}

}

GraphPresentationModel::GraphPresentationModel() :
        asyncState(std::make_shared<AsyncState>()) {
}

GraphPresentationModel::~GraphPresentationModel() {
    asyncState->alive.store(false);
    asyncState->generation.fetch_add(1);
    asyncWorker.shutdown();
}

int GraphPresentationModel::auditionMidiNoteForProbe(
        const NodeGraph& graph,
        const String& probeId,
        int fallbackMidiNote) {
    const SignalProbe* probe = findProbe(graph, probeId);
    if (probe == nullptr) {
        return fallbackMidiNote;
    }

    std::vector<String> pending { probe->sourceNodeId };
    std::set<String> visited;
    while (!pending.empty()) {
        const String nodeId = pending.back();
        pending.pop_back();
        if (!visited.insert(nodeId).second) {
            continue;
        }

        const Node* node = graph.findNode(nodeId);
        if (node != nullptr && node->kind == NodeKind::VoiceContext) {
            const auto note = attachedKeyNote(graph, nodeId, fallbackMidiNote);
            if (note.has_value()) {
                return *note;
            }
        }

        for (const auto& edge : graph.getEdges()) {
            if (edge.destNodeId == nodeId && !edge.isAttachment()) {
                pending.push_back(edge.sourceNodeId);
            }
        }
    }

    return fallbackMidiNote;
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
    if (!compile && change.probesChanged) {
        compiler.refreshSignalProbes(graph, next.compileResult.plan);
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
    const auto updateResult = updateGraph.executeDeferredPublication(
            next.compileResult.plan,
            request,
            [&](const auto& products) {
                return renderPreviewProducts(
                        graph,
                        next,
                        products,
                        compile,
                        previewRendered);
            });
    updateGraph.publish(request, updateResult);
    if (previewRendered) {
        ++previewRenders;
    }

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

    return renderPreviewProducts(
            refresh.graph,
            next,
            products,
            false,
            refresh.previewRendered,
            [&] { return isCurrent(refresh); });
}

bool GraphPresentationModel::renderPreviewProducts(
        const NodeGraph& graph,
        GraphPresentationSnapshot& snapshot,
        const std::vector<PlannedNodeProduct>& products,
        bool renderFullGraph,
        bool& previewRendered,
        GraphAudioExecutor::CancellationCheck cancellationCheck) {
    previewRendered = false;
    const bool hasPreviewTraversal = std::any_of(
            products.begin(), products.end(), [](const auto& product) {
                return product.product == UpdateProduct::PreviewTraversal;
            });
    if (!hasPreviewTraversal || !snapshot.compileResult.succeeded()) {
        return true;
    }

    constexpr size_t previewFrameCount = 128;
    const AudioExecutionSpec spec {
            previewFrameCount,
            44100.0,
            ChannelLayout::LinkedStereo
    };
    previewAudioExecutor.prepareExecution(snapshot.compileResult.plan, spec);
    if (renderFullGraph) {
        const GraphAudioResult audio = previewAudioExecutor.process(
                graph,
                snapshot.compileResult.plan,
                previewFrameCount);
        snapshot.previewResult = GraphPreviewExecutor().render(
                snapshot.compileResult.plan,
                audio,
                graph.getSignalProbes(),
                40);
        previewRendered = true;
        return true;
    }

    std::vector<uint8_t> dirtyNodes(snapshot.compileResult.plan.steps.size());
    for (const auto& product : products) {
        if (product.product != UpdateProduct::PreviewTraversal) {
            continue;
        }
        const auto step = snapshot.compileResult.plan.dependencyIndex.stepIndexById.find(
                product.nodeId);
        if (step != snapshot.compileResult.plan.dependencyIndex.stepIndexById.end()) {
            dirtyNodes[static_cast<size_t>(step->second)] = 1;
        }
    }
    const GraphAudioResultView audio = previewAudioExecutor.processIncrementalIndexed(
            graph,
            snapshot.compileResult.plan,
            previewFrameCount,
            dirtyNodes,
            cancellationCheck);
    if (audio.cancelled || (cancellationCheck && !cancellationCheck())) {
        return false;
    }
    GraphPreviewExecutor().renderIncremental(
            snapshot.compileResult.plan,
            audio,
            graph.getSignalProbes(),
            dirtyNodes,
            40,
            snapshot.previewResult);
    previewRendered = true;
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

std::optional<GraphPreviewResult::SignalProbePreview>
GraphPresentationModel::captureProbePreview(
        const NodeGraph& graph,
        const String& probeId,
        size_t frameCount,
        int midiNote) const {
    if (!current.compileResult.succeeded() || frameCount == 0) {
        return std::nullopt;
    }

    const GraphPreviewResult previews = captureProbePreviews(
            graph,
            current.compileResult.plan,
            frameCount,
            midiNote);
    const auto found = std::find_if(
            previews.probes.begin(),
            previews.probes.end(),
            [&](const auto& preview) {
                return preview.probeId == probeId;
            });
    if (found == previews.probes.end() || !found->connected) {
        return std::nullopt;
    }

    return *found;
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
        const String key = configurationFactory.keyFor(
                step.audioRole, step.parameters, node->model, spec);
        if (step.configuration.key == key) {
            continue;
        }
        auto value = configurationFactory.create(
                step.audioRole, step.parameters, node->model, spec);
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
        if (node->model != nullptr) {
            nodeFingerprint.add(node->model->schemaId()).add(node->model->revision());
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
