#include "Runtime/ProbeExecutionScope.h"

namespace CycleV2 {

std::vector<uint8_t> probeExecutionSteps(
        const GraphExecutionPlan& plan,
        int sourceStepIndex) {
    std::vector<uint8_t> active(plan.steps.size());
    std::vector<int> pending { sourceStepIndex };
    while (!pending.empty()) {
        const int stepIndex = pending.back();
        pending.pop_back();
        if (stepIndex < 0 || (size_t) stepIndex >= plan.steps.size()
                || active[(size_t) stepIndex] != 0) {
            continue;
        }
        active[(size_t) stepIndex] = 1;
        const auto& step = plan.steps[(size_t) stepIndex];
        const auto node = plan.dependencyIndex.nodeIndexById.find(step.nodeId);
        if (node != plan.dependencyIndex.nodeIndexById.end()) {
            for (const int dependency : plan.dependencyIndex.dependencies[(size_t) node->second]) {
                const auto source = plan.dependencyIndex.stepIndexById.find(
                        plan.dependencyIndex.nodeIds[(size_t) dependency]);
                if (source != plan.dependencyIndex.stepIndexById.end()) {
                    pending.push_back(source->second);
                }
            }
        }
        if (step.oscillatorRegionIndex >= 0
                && (size_t) step.oscillatorRegionIndex < plan.oscillatorRegions.size()) {
            for (const int regionStep : plan.oscillatorRegions[
                    (size_t) step.oscillatorRegionIndex].stepIndices) {
                pending.push_back(regionStep);
            }
        }
    }
    return active;
}

}
