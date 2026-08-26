#include "Runtime/PreviewPitchResolver.h"

#include "Graph/NodeParameterMap.h"

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <set>

namespace CycleV2 {

namespace {

int attachedKeyNote(
        const NodeGraph& graph,
        const String& voiceContextId,
        int fallbackMidiNote) {
    const Range<int> midiRange {
            Constants::LowestMidiNote,
            Constants::HighestMidiNote
    };
    const float fallbackKey = Arithmetic::getUnitValueForGraphicNote(
            fallbackMidiNote,
            midiRange);

    for (const auto& edge : graph.getEdges()) {
        if (edge.destNodeId != voiceContextId
                || edge.attachmentType != AttachmentType::ModulationTriple) {
            continue;
        }
        const Node* triple = graph.findNode(edge.sourceNodeId);
        if (triple == nullptr || triple->kind != NodeKind::ModulationTriple) {
            continue;
        }

        const float key = NodeParameterMap(*triple).floatValue(
                "redConstant",
                fallbackKey);
        return Arithmetic::getGraphicNoteForValue(key, midiRange);
    }

    return fallbackMidiNote;
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

int PreviewPitchResolver::forGraph(const NodeGraph& graph) {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::VoiceContext) {
            return attachedKeyNote(graph, node.id, defaultMidiNote);
        }
    }

    return defaultMidiNote;
}

int PreviewPitchResolver::forNode(
        const NodeGraph& graph,
        const String& nodeId,
        int fallbackMidiNote) {
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
            return attachedKeyNote(graph, currentId, fallbackMidiNote);
        }

        for (const auto& edge : graph.getEdges()) {
            if (edge.destNodeId == currentId && !edge.isAttachment()) {
                pending.push_back(edge.sourceNodeId);
            }
        }
    }

    return fallbackMidiNote;
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
