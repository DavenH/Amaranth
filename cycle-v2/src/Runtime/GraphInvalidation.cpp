#include "GraphInvalidation.h"

#include <cstdint>

namespace CycleV2 {

GraphInvalidationResult GraphInvalidation::invalidateFrom(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        ParameterImpact impacts) const {
    GraphInvalidationResult result;
    result.requiresRecompile = hasImpact(impacts, ParameterImpact::GraphSemantics);

    const bool dirtiesAudio = result.requiresRecompile
            || hasImpact(impacts, ParameterImpact::DspConfiguration)
            || hasImpact(impacts, ParameterImpact::ProcessorReset);
    const bool dirtiesPreview = dirtiesAudio
            || hasImpact(impacts, ParameterImpact::Preview);

    if (dirtiesAudio) {
        result.audioNodes = dependentsFrom(plan.dependencyIndex, nodeId, result);
        if (dirtiesPreview) {
            result.previewNodes = result.audioNodes;
        }
    } else if (dirtiesPreview) {
        result.previewNodes = dependentsFrom(plan.dependencyIndex, nodeId, result);
    }

    return result;
}

std::vector<String> GraphInvalidation::dependentsFrom(
        const GraphDependencyIndex& index,
        const String& nodeId,
        GraphInvalidationResult& diagnostics) const {
    int root = -1;
    for (int i = 0; i < (int) index.nodeIds.size(); ++i) {
        if (index.nodeIds[(size_t) i] == nodeId) {
            root = i;
            break;
        }
    }

    if (root < 0) {
        return {};
    }

    std::vector<uint8_t> visited(index.nodeIds.size());
    std::vector<int> pending { root };
    visited[(size_t) root] = 1;
    while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        ++diagnostics.visitedNodeCount;

        for (const int destination : index.dependents[(size_t) current]) {
            ++diagnostics.inspectedDependencyCount;
            if (visited[(size_t) destination] == 0) {
                visited[(size_t) destination] = 1;
                pending.push_back(destination);
            }
        }
    }

    std::vector<String> nodes;
    nodes.reserve(diagnostics.visitedNodeCount);
    for (int i = 0; i < (int) index.nodeIds.size(); ++i) {
        if (visited[(size_t) i] != 0) {
            nodes.push_back(index.nodeIds[(size_t) i]);
        }
    }

    return nodes;
}

}
