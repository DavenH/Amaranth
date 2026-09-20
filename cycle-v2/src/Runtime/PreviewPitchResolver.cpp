#include "Runtime/PreviewPitchResolver.h"

#include "Graph/GraphCompiler.h"
#include "Graph/NodeParameterMap.h"
#include "Nodes/Control/ModulationTriple.h"

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <array>
#include <set>

namespace CycleV2 {

namespace {

PreviewPitchBinding bindingFromTriple(
        const Node& triple,
        const String& voiceContextId) {
    const Range<int> midiRange {
            Constants::LowestMidiNote,
            Constants::HighestMidiNote
    };
    const NodeParameterMap parameters(triple);
    std::optional<int> attachedMidiNote;
    if (parameters.contains("redConstant")) {
        attachedMidiNote = Arithmetic::getGraphicNoteForValue(
                parameters.floatValue("redConstant"), midiRange);
    }
    const auto configuration = buildModulationTripleConfiguration(
            triple.parameters);
    const std::array<String, 3> axes { "yellow", "red", "blue" };
    String keyScaleAxis;
    for (size_t index = 0; index < axes.size(); ++index) {
        if (configuration->sources[index].mode == ModulationSourceMode::KeyScale) {
            keyScaleAxis = axes[index];
            break;
        }
    }
    return {
            attachedMidiNote,
            keyScaleAxis,
            voiceContextId,
            triple.id
    };
}

PreviewPitchBinding attachedPitchBinding(
        const NodeGraph& graph,
        const String& voiceContextId) {
    for (const auto& edge : graph.getEdges()) {
        if (edge.destNodeId != voiceContextId
                || edge.attachmentType != AttachmentType::ModulationTriple) {
            continue;
        }
        const Node* triple = graph.findNode(edge.sourceNodeId);
        if (triple == nullptr || triple->kind != NodeKind::ModulationTriple) {
            continue;
        }

        return bindingFromTriple(*triple, voiceContextId);
    }

    return { std::nullopt, {}, voiceContextId, {} };
}

const SignalProbe* findProbe(const NodeGraph& graph, const String& probeId) {
    const auto found = std::find_if(
            graph.getSignalProbes().begin(),
            graph.getSignalProbes().end(),
            [&](const auto& candidate) {
                return candidate.id == probeId;
            });
    return found == graph.getSignalProbes().end() ? nullptr : &*found;
}

}

PreviewPitchContext PreviewPitchBinding::contextForFallback(int fallbackMidiNote) const {
    return {
            attachedMidiNote.value_or(jlimit(0, 127, fallbackMidiNote)),
            keyScaleAxis
    };
}

PreviewPitchContext PreviewPitchBinding::contextForPreviewNote(int previewMidiNote) const {
    const int selectedNote = jlimit(0, 127, previewMidiNote);
    return {
            keyScaleAxis.isNotEmpty()
                    ? selectedNote
                    : attachedMidiNote.value_or(selectedNote),
            keyScaleAxis
    };
}

void PreviewPitchBinding::refreshFromModulationNode(const Node& node) {
    *this = bindingFromTriple(node, voiceContextNodeId);
}

int PreviewPitchResolver::forGraph(const NodeGraph& graph) {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::VoiceContext) {
            return attachedPitchBinding(graph, node.id)
                    .contextForFallback(defaultMidiNote).midiNote;
        }
    }

    return defaultMidiNote;
}

int PreviewPitchResolver::forNode(
        const NodeGraph& graph,
        const String& nodeId,
        int fallbackMidiNote) {
    return contextForNode(graph, nodeId, fallbackMidiNote).midiNote;
}

PreviewPitchContext PreviewPitchResolver::contextForNode(
        const NodeGraph& graph,
        const String& nodeId,
        int fallbackMidiNote) {
    return bindingForNode(graph, nodeId).contextForFallback(fallbackMidiNote);
}

PreviewPitchBinding PreviewPitchResolver::bindingForNode(
        const NodeGraph& graph,
        const String& nodeId) {
    const Node* target = graph.findNode(nodeId);
    std::vector<String> pending { nodeId };
    std::set<String> visited;

    while (!pending.empty()) {
        const String currentId = pending.back();
        pending.pop_back();
        if (!visited.insert(currentId).second) {
            continue;
        }

        const Node* node = graph.findNode(currentId);
        if (node != nullptr && node->kind == NodeKind::VoiceContext) {
            return attachedPitchBinding(graph, currentId);
        }

        for (const auto& edge : graph.getEdges()) {
            if (edge.destNodeId == currentId && !edge.isAttachment()) {
                pending.push_back(edge.sourceNodeId);
            }
        }
    }

    if (target != nullptr && target->kind == NodeKind::TrilinearMesh) {
        for (const auto& edge : GraphCompiler::implicitVoiceContextEdges(graph)) {
            if (edge.destNodeId == nodeId) {
                return attachedPitchBinding(
                        graph, edge.sourceNodeId);
            }
        }
    }

    return {};
}

PreviewPitchContext PreviewPitchResolver::contextForNodeAtPreviewNote(
        const NodeGraph& graph,
        const String& nodeId,
        int previewMidiNote) {
    return bindingForNode(graph, nodeId).contextForPreviewNote(previewMidiNote);
}

int PreviewPitchResolver::forProbe(
        const NodeGraph& graph,
        const String& probeId,
        int fallbackMidiNote) {
    const SignalProbe* probe = findProbe(graph, probeId);
    return probe != nullptr
            ? forNode(graph, probe->sourceNodeId, fallbackMidiNote)
            : fallbackMidiNote;
}

}
