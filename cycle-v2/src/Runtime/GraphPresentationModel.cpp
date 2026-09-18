#include <algorithm>

#include "Runtime/GraphPresentationModel.h"
#include "Runtime/PreviewPitchResolver.h"

#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Control/ModulationTriple.h"

namespace CycleV2 {

namespace {

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

}

GraphPresentationModel::~GraphPresentationModel() {
    scheduler.shutdown();
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
        scheduler.cancelAndWait();
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
        scheduler.clearProductCache();
        previewRenderer.resetExecutionState();
    } else if (change.guidesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration)) {
        refreshConfigurations(graph, next.compileResult.plan, change.nodeIds);
    }
    if (!compile && change.probesChanged) {
        compiler.refreshSignalProbes(graph, next.compileResult.plan);
    }

    bool previewRendered {};
    const auto request = scheduler.request(
            graph,
            next.compileResult.plan,
            documentRevision,
            change,
            { current.graphRevision, current.previewMidiNote,
                    current.previewModWheelValue },
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
    scheduler.executeSynchronous(
            next.compileResult.plan,
            request,
            [&](const auto& products) {
                return previewRenderer.render(
                        graph,
                        next,
                        products,
                        compile,
                        PresentationRefreshScope::Downstream,
                        previewRendered,
                        performance);
            });
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

    scheduler.cancelAndWait();
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

    scheduler.cancelAndWait();
    current.previewModWheelValue = selectedValue;

    return refreshPreviewControls(
            graph,
            documentRevision,
            modWheelPreviewRootNodeIds);
}

void GraphPresentationModel::stagePreviewModWheelValue(int value) {
    current.previewModWheelValue = jlimit(0, 127, value);
}

GraphChangeSet GraphPresentationModel::modWheelPreviewChange(
        GraphChangeSet change) const {
    change.parameterImpacts = change.parameterImpacts | ParameterImpact::Preview;
    for (const auto& nodeId : modWheelPreviewRootNodeIds) {
        if (std::find(change.nodeIds.begin(), change.nodeIds.end(), nodeId)
                == change.nodeIds.end()) {
            change.nodeIds.push_back(nodeId);
        }
    }
    return change;
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
    const uint64_t generation = scheduler.beginAsyncRequest();
    const bool preview = requiresPreview(change);
    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    const auto request = scheduler.request(
            *graph,
            next.compileResult.plan,
            documentRevision,
            change,
            { current.graphRevision, current.previewMidiNote,
                    current.previewModWheelValue },
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

    AsyncRefresh refresh;
    refresh.graph = std::move(graph);
    refresh.change = std::move(change);
    refresh.scope = scope;
    refresh.request = request;
    refresh.snapshot = std::move(next);
    refresh.completion = std::move(completion);
    refresh.requestedAtMicroseconds = requestedAt;
    scheduler.enqueue(
            generation,
            std::move(refresh),
            performance,
            [this](AsyncRefresh& job, const auto& products) {
                return executeAsyncProducts(job, products);
            },
            [this](AsyncRefresh& job) {
                if (!acceptSnapshot(std::move(job.snapshot))) {
                    return false;
                }
                if (job.scope == PresentationRefreshScope::Downstream
                        && (job.change.guidesChanged
                                || hasImpact(job.change.parameterImpacts,
                                        ParameterImpact::DspConfiguration))) {
                    modWheelPreviewRootNodeIds = modWheelPreviewRoots(current.compileResult.plan);
                    ++audioRevision;
                }
                if (job.previewRendered) {
                    ++previewRenders;
                }
                return true;
            });
}

bool GraphPresentationModel::executeAsyncProducts(
        AsyncRefresh& refresh,
        const std::vector<PlannedNodeProduct>& products) {
    auto& next = refresh.snapshot;
    const bool preparesConfiguration = std::any_of(
            products.begin(), products.end(), [](const auto& product) {
                return product.product == UpdateProduct::AudioConfiguration;
            });
    if (preparesConfiguration
            || (refresh.scope != PresentationRefreshScope::Downstream
                    && hasImpact(refresh.change.parameterImpacts,
                            ParameterImpact::DspConfiguration))) {
        const uint64_t startedAt = performance.timestamp();
        refreshConfigurations(*refresh.graph, next.compileResult.plan, refresh.change.nodeIds);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::Configuration,
                performance.timestamp() - startedAt);
    }
    if (!scheduler.isCurrent(refresh) || !requiresPreview(refresh.change)
            || !next.compileResult.succeeded()) {
        return scheduler.isCurrent(refresh);
    }

    return previewRenderer.render(
            *refresh.graph,
            next,
            products,
            false,
            refresh.scope,
            refresh.previewRendered,
            performance,
            [&] { return scheduler.isCurrent(refresh); });
}

void GraphPresentationModel::recordEditorMovement(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        bool deferredUntilCommit) {
    scheduler.recordEditorMovement(
            current.compileResult.plan,
            nodeId,
            field,
            effectiveFingerprint,
            deferredUntilCommit);
}

void GraphPresentationModel::commitLocalEditorState(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        uint64_t documentRevision) {
    if (!scheduler.commitLocalEditorState(
                current.compileResult.plan,
                nodeId,
                field,
                effectiveFingerprint)) {
        return;
    }
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
    return previewRenderer.captureProbePreview(
            graph,
            current.compileResult.plan,
            probeId,
            rasterRowCount,
            midiNote,
            current.previewModWheelValue);
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

}
