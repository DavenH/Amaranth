#include "Runtime/PreviewPitchContextIndex.h"

#include <set>

namespace CycleV2 {

void PreviewPitchContextIndex::rebuild(const NodeGraph& graph) {
    bindings.clear();
    dependentsByModulationNode.clear();
    initialized = true;
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::TrilinearMesh) {
            continue;
        }
        PreviewPitchBinding binding = PreviewPitchResolver::bindingForNode(
                graph, node.id);
        if (binding.modulationNodeId.isNotEmpty()) {
            dependentsByModulationNode[binding.modulationNodeId].push_back(node.id);
        }
        bindings.emplace(node.id, std::move(binding));
        ++graphResolutions;
    }
}

void PreviewPitchContextIndex::applyParameterChanges(
        const NodeGraph& graph,
        const std::vector<String>& changedNodeIds,
        bool topologyChanged) {
    if (topologyChanged || !initialized) {
        rebuild(graph);
        return;
    }

    std::set<String> refreshed;
    for (const String& nodeId : changedNodeIds) {
        if (!refreshed.insert(nodeId).second) {
            continue;
        }
        const auto dependents = dependentsByModulationNode.find(nodeId);
        if (dependents == dependentsByModulationNode.end()) {
            continue;
        }
        const Node* node = graph.findNode(nodeId);
        if (node == nullptr || node->kind != NodeKind::ModulationTriple) {
            continue;
        }
        for (const String& dependentId : dependents->second) {
            bindings.at(dependentId).refreshFromModulationNode(*node);
        }
        ++parameterRefreshes;
    }
}

PreviewPitchContext PreviewPitchContextIndex::contextForNodeAtPreviewNote(
        const String& nodeId,
        int previewMidiNote) const {
    const auto found = bindings.find(nodeId);
    return found != bindings.end()
            ? found->second.contextForPreviewNote(previewMidiNote)
            : PreviewPitchContext { jlimit(0, 127, previewMidiNote), {} };
}

}
