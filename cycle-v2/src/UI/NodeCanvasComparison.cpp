#include <algorithm>

#include "UI/NodeCanvas.h"

#include "Graph/DefaultOutputProbeResolver.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

void NodeCanvas::setComparisonRenderCallbacks(
        std::function<void(float)> renderCallback,
        std::function<void()> closingCallback) {
    const juce::ScopedLock lock(comparisonRenderLock);
    comparisonRenderCallback = std::move(renderCallback);
    comparisonGlClosingCallback = std::move(closingCallback);
}

void NodeCanvas::clearComparisonRenderCallbacks() {
    if (canvasOpenGlAttached) {
        openGLContext.executeOnGLThread([this](juce::OpenGLContext&) {
            const juce::ScopedLock lock(comparisonRenderLock);
            if (comparisonGlClosingCallback) {
                comparisonGlClosingCallback();
            }
        }, true);
    }
    const juce::ScopedLock lock(comparisonRenderLock);
    comparisonRenderCallback = {};
    comparisonGlClosingCallback = {};
}

void NodeCanvas::requestComparisonRendering() {
    if (canvasOpenGlAttached) {
        openGLContext.triggerRepaint();
    }
}

void NodeCanvas::refreshComparisonAfterCommit() {
    if (!document.presentation().comparisonSpyPins.empty() && comparisonRefreshCallback) {
        comparisonRefreshPending = false;
        comparisonRefreshCallback();
    }
}

bool NodeCanvas::hasTrimeshEditorOpen() const {
    const Node* node = graph.findNode(expandedNodeId);
    return node != nullptr && node->kind == NodeKind::TrilinearMesh;
}

std::vector<ComparisonSpyOption> NodeCanvas::comparisonSpyOptions() const {
    std::vector<ComparisonSpyOption> options;
    options.push_back({ DefaultOutputProbeResolver::probeId, "Output Spy (pre-wet)", false });
    for (const auto& probe : graph.getSignalProbes()) {
        options.push_back({
                probe.id,
                probe.label.isNotEmpty() ? probe.label : probe.id,
                false
        });
    }
    return options;
}

std::vector<ComparisonSpyOption> NodeCanvas::unprobedTrimeshOutputs() const {
    std::vector<ComparisonSpyOption> options;
    for (const auto& edge : graph.getEdges()) {
        if (edge.isAttachment()
                || graph.findSignalProbeForSource(
                        edge.sourceNodeId, edge.sourcePortId) != nullptr) {
            continue;
        }
        const Node* source = graph.findNode(edge.sourceNodeId);
        if (source == nullptr || source->kind != NodeKind::TrilinearMesh) {
            continue;
        }
        const auto found = std::find_if(options.begin(), options.end(), [&](const auto& option) {
            return option.id == source->id;
        });
        if (found == options.end()) {
            options.push_back({
                    source->id,
                    (source->subtitle.isNotEmpty() ? source->subtitle : source->id) + " output",
                    false
            });
        }
    }
    return options;
}

bool NodeCanvas::pinComparisonSpy(const String& probeId) {
    if (!commands.pinComparisonSpy(probeId)) {
        return false;
    }
    notifyGraphDocumentStateChanged();
    if (comparisonRefreshCallback) {
        comparisonRefreshCallback();
    }
    return true;
}

bool NodeCanvas::unpinComparisonSpy(const String& probeId) {
    if (!commands.unpinComparisonSpy(probeId)) {
        return false;
    }
    notifyGraphDocumentStateChanged();
    if (comparisonRefreshCallback) {
        comparisonRefreshCallback();
    }
    return true;
}

String NodeCanvas::createAndPinTrimeshOutputSpy(const String& nodeId) {
    const Node* node = graph.findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return {};
    }
    const auto& edges = graph.getEdges();
    for (size_t index = 0; index < edges.size(); ++index) {
        const Edge& edge = edges[index];
        if (edge.isAttachment() || edge.sourceNodeId != nodeId) {
            continue;
        }
        const SignalProbe* existing = graph.findSignalProbeForSource(
                edge.sourceNodeId, edge.sourcePortId);
        if (existing != nullptr) {
            pinComparisonSpy(existing->id);
            return existing->id;
        }
        if (!applyAuthoringResult(authoring.toggleSignalProbe((int) index, 0.5f))) {
            continue;
        }
        const SignalProbe* created = graph.findSignalProbeForSource(
                edge.sourceNodeId, edge.sourcePortId);
        if (created != nullptr) {
            pinComparisonSpy(created->id);
            return created->id;
        }
    }
    return {};
}

std::vector<std::optional<GraphPreviewResult::SignalProbePreview>>
NodeCanvas::captureComparisonSpies(int midiNote) {
    const auto& pins = document.presentation().comparisonSpyPins;
    return presentation.captureProbePreviews(
            graph,
            pins,
            GraphPreviewExecutor::sourceFrameCountForMidiNote(midiNote),
            midiNote);
}

}
