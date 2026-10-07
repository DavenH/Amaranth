#include <algorithm>
#include <memory>
#include <vector>

#include "Graph/CompiledProbeIndexBuilder.h"
#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/GraphCompiler.h"

namespace CycleV2 {

void CompiledProbeIndexBuilder::refresh(
        const NodeGraph& graph,
        GraphExecutionPlan& plan) {
    auto observations = std::make_shared<GraphObservationIndex>();
    observations->observedNodes.resize(plan.dependencyIndex.nodeIds.size());
    observations->leadsToObservation.resize(plan.dependencyIndex.nodeIds.size());
    observations->probeIndicesByStep.resize(plan.steps.size());
    observations->hasProbes = !graph.getSignalProbes().empty();
    std::vector<int> pending;
    plan.signalProbes.clear();
    plan.signalProbes.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        CompiledSignalProbe compiled;
        compiled.probeId = probe.id;
        const auto source = plan.dependencyIndex.stepIndexById.find(probe.sourceNodeId);
        if (source != plan.dependencyIndex.stepIndexById.end()) {
            compiled.sourceStepIndex = source->second;
            observations->probeIndicesByStep[static_cast<size_t>(source->second)]
                    .push_back(plan.signalProbes.size());
            const auto observed = plan.dependencyIndex.nodeIndexById.find(probe.sourceNodeId);
            if (observed != plan.dependencyIndex.nodeIndexById.end()) {
                const auto index = static_cast<size_t>(observed->second);
                observations->observedNodes[index] = 1;
                if (observations->leadsToObservation[index] == 0) {
                    observations->leadsToObservation[index] = 1;
                    pending.push_back(observed->second);
                }
            }
            const auto& outputs = plan.steps[static_cast<size_t>(source->second)].outputs;
            const auto output = std::find_if(
                    outputs.begin(), outputs.end(),
                    [&](const auto& candidate) {
                        return candidate.portId == probe.sourcePortId;
                    });
            if (output != outputs.end()) {
                compiled.sourceOutputIndex = static_cast<int>(
                        std::distance(outputs.begin(), output));
            }
        }
        plan.signalProbes.push_back(std::move(compiled));
    }
    while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        for (const int dependency :
                plan.dependencyIndex.dependencies[static_cast<size_t>(current)]) {
            const auto index = static_cast<size_t>(dependency);
            if (observations->leadsToObservation[index] == 0) {
                observations->leadsToObservation[index] = 1;
                pending.push_back(dependency);
            }
        }
    }
    plan.observationIndex = std::move(observations);

    plan.defaultOutputProbe.reset();
    const auto address = DefaultOutputProbeResolver().resolve(graph);
    if (!address.has_value()) {
        return;
    }
    CompiledSignalProbe compiled;
    compiled.probeId = DefaultOutputProbeResolver::probeId;
    const auto source = plan.dependencyIndex.stepIndexById.find(address->sourceNodeId);
    if (source == plan.dependencyIndex.stepIndexById.end()) {
        return;
    }
    compiled.sourceStepIndex = source->second;
    const auto& outputs = plan.steps[static_cast<size_t>(source->second)].outputs;
    const auto output = std::find_if(
            outputs.begin(), outputs.end(),
            [&](const auto& candidate) {
                return candidate.portId == address->sourcePortId;
            });
    if (output == outputs.end()) {
        return;
    }
    compiled.sourceOutputIndex = static_cast<int>(std::distance(outputs.begin(), output));
    plan.defaultOutputProbe = std::move(compiled);
}

}
