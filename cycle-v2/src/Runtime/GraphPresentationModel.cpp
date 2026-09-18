#include <algorithm>

#include <App/AppConstants.h>

#include "Runtime/GraphPresentationModel.h"
#include "Runtime/FingerprintBuilder.h"
#include "Runtime/PreviewPitchResolver.h"

#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Control/ModulationTriple.h"

namespace CycleV2 {

namespace {

constexpr size_t kCompactPreviewFrameCount = 512;
constexpr size_t kExpandedProbeColumnCount = 512;
constexpr size_t kMaximumExpandedProbeRows = 512;

bool usesModWheel(const ModulationSourceConfiguration& configuration) {
    return configuration.mode == ModulationSourceMode::ModWheel
            || (configuration.mode == ModulationSourceMode::MidiController
                    && configuration.controller == 1);
}

std::vector<String> modWheelPreviewRoots(const GraphExecutionPlan& plan) {
    std::vector<String> roots;
    const auto appendRoot = [&](const String& nodeId) {
        if (std::find(roots.begin(), roots.end(), nodeId) == roots.end()) {
            roots.push_back(nodeId);
        }
    };
    for (const auto& step : plan.steps) {
        if (const auto source = std::dynamic_pointer_cast<
                    const ModulationSourceConfiguration>(step.configuration.value)) {
            if (usesModWheel(*source)) {
                appendRoot(step.nodeId);
            }
            continue;
        }
        const auto triple = std::dynamic_pointer_cast<
                const ModulationTripleConfiguration>(step.configuration.value);
        if (triple != nullptr
                && std::any_of(
                        triple->sources.begin(),
                        triple->sources.end(),
                        usesModWheel)) {
            appendRoot(step.nodeId);
        }
    }
    for (const auto& context : plan.voiceContexts) {
        const auto triple = std::dynamic_pointer_cast<
                const ModulationTripleConfiguration>(context.defaultModulation);
        if (triple != nullptr
                && std::any_of(
                        triple->sources.begin(),
                        triple->sources.end(),
                        usesModWheel)) {
            appendRoot(context.nodeId);
        }
    }
    return roots;
}

GraphPreviewResult captureProbePreviews(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        int midiNote,
        int modWheelValue) {
    GraphAudioExecutor captureExecutor;

    AudioVoiceContext voice;
    voice.controls.noteNumber = jlimit(0, 127, midiNote);
    voice.controls.controllers[1] = (float) jlimit(0, 127, modWheelValue) / 127.f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const GraphAudioResult audio = captureExecutor.process(
            graph,
            plan,
            frameCount,
            {},
            voice,
            kExpandedProbeColumnCount);
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
    return PreviewPitchResolver::forProbe(graph, probeId, fallbackMidiNote);
}

bool GraphPresentationModel::refresh(
        const NodeGraph& graph,
        uint64_t documentRevision,
        const GraphChangeSet& change) {
    using Performance = GraphPresentationPerformanceMetrics;
    const uint64_t startedAt = performance.timestamp();
    performance.record(Performance::Outcome::Requested);
    requestedGraphRevision = documentRevision;
    const bool compile = current.graphRevision == 0 || requiresCompilation(change);
    const bool preview = compile || requiresPreview(change);
    if (compile) {
        asyncState->generation.fetch_add(1);
        asyncWorker.cancelAndWait();
    }

    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    if (!hasExplicitPreviewMidiNote) {
        next.previewMidiNote = PreviewPitchResolver::forGraph(graph);
    }
    if (compile) {
        next.compileResult = compiler.compile(graph);
        next.runtimeTrace = {};
        ++compilations;
        if (next.compileResult.succeeded()) {
            next.runtimeTrace = GraphRuntime().process(graph, next.compileResult.plan);
        }
        updateGraph.clearProductCache();
        previewAudioExecutor.resetExecutionState();
    } else if (change.guidesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)) {
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
            preview,
            PresentationRefreshScope::Downstream);
    if (!compile && !request.edit.isValid()) {
        performance.record(
                Performance::Stage::SynchronousRefresh,
                performance.timestamp() - startedAt);
        performance.record(Performance::Outcome::NoWork);
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
                        PresentationRefreshScope::Downstream,
                        previewRendered);
            });
    updateGraph.publish(request, updateResult);
    if (previewRendered) {
        ++previewRenders;
    }

    const bool accepted = acceptSnapshot(std::move(next));
    if (accepted && (compile
            || change.guidesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration))) {
        modWheelPreviewRootNodeIds = modWheelPreviewRoots(current.compileResult.plan);
        ++audioRevision;
    }
    performance.record(
            Performance::Stage::SynchronousRefresh,
            performance.timestamp() - startedAt);
    performance.record(accepted
            ? Performance::Outcome::Published
            : Performance::Outcome::StaleOrCancelled);
    return accepted;
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

bool GraphPresentationModel::refreshPreviewMidiNote(
        const NodeGraph& graph,
        uint64_t documentRevision,
        int midiNote) {
    const int selectedNote = jlimit(0, 127, midiNote);
    if (current.previewMidiNote == selectedNote) {
        return true;
    }

    asyncState->generation.fetch_add(1);
    asyncWorker.cancelAndWait();
    current.previewMidiNote = selectedNote;
    hasExplicitPreviewMidiNote = true;

    std::vector<String> roots;
    roots.reserve(graph.getNodes().size());
    for (const auto& node : graph.getNodes()) {
        roots.push_back(node.id);
    }
    return refreshPreviewControls(graph, documentRevision, std::move(roots));
}

bool GraphPresentationModel::refreshPreviewModWheelValue(
        const NodeGraph& graph,
        uint64_t documentRevision,
        int value) {
    const int selectedValue = jlimit(0, 127, value);
    if (current.previewModWheelValue == selectedValue) {
        return true;
    }

    asyncState->generation.fetch_add(1);
    asyncWorker.cancelAndWait();
    current.previewModWheelValue = selectedValue;

    return refreshPreviewControls(
            graph,
            documentRevision,
            modWheelPreviewRootNodeIds);
}

void GraphPresentationModel::stagePreviewModWheelValue(int value) {
    current.previewModWheelValue = jlimit(0, 127, value);
}

bool GraphPresentationModel::refreshPreviewModWheelValueAsync(
        std::shared_ptr<const NodeGraph> graph,
        uint64_t documentRevision,
        int value,
        std::function<void()> completion) {
    const int selectedValue = jlimit(0, 127, value);
    if (graph == nullptr || current.previewModWheelValue == selectedValue) {
        return graph != nullptr;
    }

    current.previewModWheelValue = selectedValue;
    GraphChangeSet change;
    change.parameterImpacts = ParameterImpact::Preview;
    change.nodeIds = modWheelPreviewRootNodeIds;
    refreshAsync(
            std::move(graph),
            documentRevision,
            std::move(change),
            PresentationRefreshScope::Downstream,
            std::move(completion));
    return true;
}

bool GraphPresentationModel::refreshPreviewControls(
        const NodeGraph& graph,
        uint64_t documentRevision,
        std::vector<String> rootNodeIds) {
    if (rootNodeIds.empty()) {
        return true;
    }

    GraphChangeSet change;
    change.parameterImpacts = ParameterImpact::Preview;
    change.nodeIds = std::move(rootNodeIds);
    return refresh(graph, documentRevision, change);
}

void GraphPresentationModel::refreshAsync(
        NodeGraph graph,
        uint64_t documentRevision,
        GraphChangeSet change,
        PresentationRefreshScope scope,
        std::function<void()> completion) {
    refreshAsync(
            std::make_shared<const NodeGraph>(std::move(graph)),
            documentRevision,
            std::move(change),
            scope,
            std::move(completion));
}

void GraphPresentationModel::refreshAsync(
        std::shared_ptr<const NodeGraph> graph,
        uint64_t documentRevision,
        GraphChangeSet change,
        PresentationRefreshScope scope,
        std::function<void()> completion) {
    using Performance = GraphPresentationPerformanceMetrics;
    const uint64_t requestedAt = performance.timestamp();
    if (graph == nullptr) {
        return;
    }
    if (current.graphRevision == 0 || requiresCompilation(change)) {
        refresh(*graph, documentRevision, change);
        performance.record(
                Performance::Stage::EndToEnd,
                performance.timestamp() - requestedAt);
        if (completion) {
            completion();
        }
        return;
    }
    performance.record(Performance::Outcome::Requested);

    requestedGraphRevision = documentRevision;
    const uint64_t generation = asyncState->generation.fetch_add(1) + 1;
    const bool preview = requiresPreview(change);
    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    const auto request = updateRequest(
            *graph,
            next.compileResult.plan,
            documentRevision,
            change,
            false,
            preview,
            scope);
    if (!request.edit.isValid() || request.invalidations.empty()) {
        acceptSnapshot(std::move(next));
        performance.record(
                Performance::Stage::EndToEnd,
                performance.timestamp() - requestedAt);
        performance.record(Performance::Outcome::NoWork);
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
        performance.record(
                Performance::Stage::EndToEnd,
                performance.timestamp() - requestedAt);
        performance.record(Performance::Outcome::NoWork);
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
    refresh->scope = scope;
    refresh->request = request;
    refresh->requestFingerprint = requestFingerprint;
    refresh->snapshot = std::move(next);
    refresh->completion = std::move(completion);
    refresh->requestedAtMicroseconds = requestedAt;
    asyncWorker.post([this, refresh] {
        using Performance = GraphPresentationPerformanceMetrics;
        const uint64_t workerStartedAt = performance.timestamp();
        performance.record(
                Performance::Stage::QueueDelay,
                workerStartedAt - refresh->requestedAtMicroseconds);
        if (!isCurrent(*refresh)) {
            updateGraph.recordDecision(
                    refresh->request, UpdateTracePhase::SupersededBeforeStart);
            performance.record(Performance::Outcome::SupersededBeforeStart);
            return false;
        }
        const bool prepared = prepareAsyncRefresh(*refresh);
        refresh->workerFinishedAtMicroseconds = performance.timestamp();
        performance.record(
                Performance::Stage::Worker,
                refresh->workerFinishedAtMicroseconds - workerStartedAt);
        if (!prepared) {
            performance.record(Performance::Outcome::StaleOrCancelled);
        }
        return prepared;
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
        const uint64_t startedAt = performance.timestamp();
        refreshConfigurations(*refresh.graph, next.compileResult.plan, refresh.change.nodeIds);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::Configuration,
                performance.timestamp() - startedAt);
    }
    if (!isCurrent(refresh) || !requiresPreview(refresh.change)
            || !next.compileResult.succeeded()) {
        return isCurrent(refresh);
    }

    return renderPreviewProducts(
            *refresh.graph,
            next,
            products,
            false,
            refresh.scope,
            refresh.previewRendered,
            [&] { return isCurrent(refresh); });
}

bool GraphPresentationModel::renderPreviewProducts(
        const NodeGraph& graph,
        GraphPresentationSnapshot& snapshot,
        const std::vector<PlannedNodeProduct>& products,
        bool renderFullGraph,
        PresentationRefreshScope scope,
        bool& previewRendered,
        GraphAudioExecutor::CancellationCheck cancellationCheck) {
    previewRendered = false;
    const bool hasPreviewWork = std::any_of(
            products.begin(), products.end(), [](const auto& product) {
                return product.product == UpdateProduct::PreviewTraversal
                        || product.product == UpdateProduct::CompactPreview
                        || product.product == UpdateProduct::ProbePreview;
            });
    if (!hasPreviewWork || !snapshot.compileResult.succeeded()) {
        return true;
    }

    const AudioExecutionSpec spec {
            kCompactPreviewFrameCount,
            44100.0,
            ChannelLayout::LinkedStereo
    };
    previewAudioExecutor.prepareExecution(snapshot.compileResult.plan, spec);
    AudioVoiceContext previewVoice;
    previewVoice.controls.noteNumber = snapshot.previewMidiNote;
    previewVoice.controls.controllers[1]
            = (float) snapshot.previewModWheelValue / 127.f;
    previewVoice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    PreviewControlContext previewControls;
    previewControls.noteNumber = snapshot.previewMidiNote;
    previewControls.controllers[1]
            = (float) snapshot.previewModWheelValue / 127.f;
    previewControls.lowestNote = Constants::LowestMidiNote;
    previewControls.highestNote = Constants::HighestMidiNote;
    previewControls.traverseVoiceTime = true;
    if (renderFullGraph) {
        const uint64_t audioStartedAt = performance.timestamp();
        const GraphAudioResult audio = previewAudioExecutor.process(
                graph,
                snapshot.compileResult.plan,
                kCompactPreviewFrameCount,
                {},
                previewVoice);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::PreviewAudio,
                performance.timestamp() - audioStartedAt);
        const uint64_t extractionStartedAt = performance.timestamp();
        snapshot.previewResult = GraphPreviewExecutor().render(
                snapshot.compileResult.plan,
                audio,
                graph.getSignalProbes(),
                40,
                &previewControls);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::PreviewExtraction,
                performance.timestamp() - extractionStartedAt);
        previewRendered = true;
        return true;
    }

    std::vector<uint8_t> dirtyNodes(snapshot.compileResult.plan.steps.size());
    for (const auto& product : products) {
        if (product.product != UpdateProduct::PreviewTraversal
                && product.product != UpdateProduct::CompactPreview) {
            continue;
        }
        const auto step = snapshot.compileResult.plan.dependencyIndex.stepIndexById.find(
                product.nodeId);
        if (step != snapshot.compileResult.plan.dependencyIndex.stepIndexById.end()) {
            dirtyNodes[static_cast<size_t>(step->second)] = 1;
        }
    }
    const uint64_t audioStartedAt = performance.timestamp();
    const GraphAudioResultView audio = previewAudioExecutor.processIncrementalIndexed(
            graph,
            snapshot.compileResult.plan,
            kCompactPreviewFrameCount,
            dirtyNodes,
            previewVoice,
            cancellationCheck);
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::PreviewAudio,
            performance.timestamp() - audioStartedAt);
    if (audio.cancelled || (cancellationCheck && !cancellationCheck())) {
        return false;
    }
    const uint64_t extractionStartedAt = performance.timestamp();
    if (scope == PresentationRefreshScope::LocalEditor) {
        GraphPreviewExecutor().renderNodePreviewsIncremental(
                snapshot.compileResult.plan,
                audio,
                dirtyNodes,
                40,
                snapshot.previewResult,
                &previewControls);
    } else {
        GraphPreviewExecutor().renderIncremental(
                snapshot.compileResult.plan,
                audio,
                graph.getSignalProbes(),
                dirtyNodes,
                40,
                snapshot.previewResult,
                &previewControls);
    }
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::PreviewExtraction,
            performance.timestamp() - extractionStartedAt);
    previewRendered = true;
    return true;
}

std::function<void()> GraphPresentationModel::publishAsyncRefresh(
        std::shared_ptr<AsyncRefresh> refresh) {
    using Performance = GraphPresentationPerformanceMetrics;
    const uint64_t publicationStartedAt = performance.timestamp();
    if (refresh->workerFinishedAtMicroseconds != 0) {
        performance.record(
                Performance::Stage::PublicationDelay,
                publicationStartedAt - refresh->workerFinishedAtMicroseconds);
    }
    if (!refresh->state->alive.load()) {
        performance.record(Performance::Outcome::StaleOrCancelled);
        return {};
    }
    if (!isCurrent(*refresh)) {
        updateGraph.recordDecision(refresh->request, UpdateTracePhase::StaleResultDiscarded);
        performance.record(Performance::Outcome::StaleOrCancelled);
        return {};
    }
    if (refresh->generation < publishedGeneration
            || !acceptSnapshot(std::move(refresh->snapshot))) {
        updateGraph.recordDecision(refresh->request, UpdateTracePhase::StaleResultDiscarded);
        performance.record(Performance::Outcome::StaleOrCancelled);
        return {};
    }
    publishedGeneration = refresh->generation;
    updateGraph.publish(refresh->request, refresh->updateResult);
    if (refresh->change.guidesChanged
            || hasImpact(refresh->change.parameterImpacts,
                    ParameterImpact::DspConfiguration)) {
        modWheelPreviewRootNodeIds = modWheelPreviewRoots(current.compileResult.plan);
        ++audioRevision;
    }
    if (refresh->previewRendered) {
        ++previewRenders;
    }
    if (refresh->scope == PresentationRefreshScope::Downstream) {
        publishedEditFingerprint = refresh->requestFingerprint;
    }
    performance.record(
            Performance::Stage::EndToEnd,
            performance.timestamp() - refresh->requestedAtMicroseconds);
    performance.record(Performance::Outcome::Published);
    return std::move(refresh->completion);
}

bool GraphPresentationModel::isCurrent(const AsyncRefresh& refresh) const {
    if (!refresh.state->alive.load()
            || refresh.generation != refresh.state->generation.load()) {
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
    const auto identity = gestureSession.recordMovement(stream, streamFingerprint);
    if (!identity.has_value()) {
        return;
    }
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
    const String stream = gestureSession.activeStreamOr("editor:" + nodeId);
    const EditIdentity identity = gestureSession.commit(stream);
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
        size_t rasterRowCount,
        int midiNote) const {
    if (!current.compileResult.succeeded() || rasterRowCount == 0) {
        return std::nullopt;
    }

    GraphPreviewResult previews = captureProbePreviews(
            graph,
            current.compileResult.plan,
            rasterRowCount,
            midiNote,
            current.previewModWheelValue);
    auto found = std::find_if(
            previews.probes.begin(),
            previews.probes.end(),
            [&](const auto& preview) {
                return preview.probeId == probeId;
            });
    if (found == previews.probes.end() || !found->connected) {
        return std::nullopt;
    }

    GraphPreviewExecutor::reduceProbeRows(
            *found,
            std::min(rasterRowCount, kMaximumExpandedProbeRows));
    return *found;
}

bool GraphPresentationModel::requiresCompilation(const GraphChangeSet& change) const {
    return change.topologyChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::GraphSemantics);
}

bool GraphPresentationModel::requiresPreview(const GraphChangeSet& change) const {
    return change.probesChanged
            || change.guidesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)
            || hasImpact(change.parameterImpacts, ParameterImpact::Preview)
            || hasImpact(change.parameterImpacts, ParameterImpact::Presentation);
}

void GraphPresentationModel::refreshConfigurations(
        const NodeGraph& graph,
        GraphExecutionPlan& plan,
        const std::vector<String>& nodeIds) {
    plan.outputGain = GraphCompiler::outputGainFor(graph);
    compiler.refreshVoiceContexts(graph, plan);
    AudioExecutionSpec spec;
    for (auto& step : plan.steps) {
        const bool directlyChanged = nodeIds.empty()
                || std::find(nodeIds.begin(), nodeIds.end(), step.nodeId) != nodeIds.end();
        if (!directlyChanged) {
            continue;
        }
        const Node* node = graph.findNode(step.nodeId);
        if (node == nullptr) {
            continue;
        }
        step.parameters = node->parameters;
        const String key = configurationFactory.keyFor(
                step.audioRole,
                step.parameters,
                node->model,
                spec,
                &graph,
                step.nodeId,
                effectiveScratchSourceNodeId(step));
        if (step.configuration.key == key) {
            continue;
        }
        auto value = configurationFactory.create(
                step.audioRole,
                step.parameters,
                node->model,
                spec,
                &graph,
                step.nodeId,
                effectiveScratchSourceNodeId(step),
                step.configuration.value.get());
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
        bool preview,
        PresentationRefreshScope scope) {
    const String stream = gestureSession.activeStreamOr(
            "graph:" + (change.nodeIds.empty() ? String("document") : change.nodeIds.front()));
    const uint64_t fingerprint = PresentationUpdateRequestBuilder::effectiveFingerprint(
            graph,
            documentRevision,
            change,
            current.previewMidiNote,
            current.previewModWheelValue);
    const EditPhase phase = documentRevision > current.graphRevision
            ? EditPhase::Commit
            : EditPhase::Movement;
    const auto identity = gestureSession.identityForRequest(stream, fingerprint, phase);
    if (!identity.has_value()) {
        return {};
    }
    return PresentationUpdateRequestBuilder::build(
            graph, plan, change, *identity, stream, fingerprint, compile, preview, scope);
}

}
