#include "Graph/GraphAudioScopeCompiler.h"

#include <algorithm>

namespace CycleV2 {

void GraphAudioScopeCompiler::applyOwnership(
        GraphExecutionPlan& plan,
        const GraphAudioScopeAnalysis& analysis) {
    for (auto& step : plan.steps) {
        if (analysis.scopeFor(step.nodeId) == AuthoredAudioScope::Global) {
            step.ownershipScope = RuntimeOwnershipScope::Global;
        } else if (step.ownershipScope == RuntimeOwnershipScope::Global) {
            step.ownershipScope = RuntimeOwnershipScope::SynthVoice;
        }
    }
}

void GraphAudioScopeCompiler::compileVoiceMixBoundary(
        GraphExecutionPlan& plan,
        const GraphAudioScopeAnalysis& analysis) {
    plan.voiceMixBufferIndices.clear();
    plan.globalInputBufferIndex = -1;
    for (const auto& step : plan.steps) {
        if (step.kind == NodeKind::GlobalInput) {
            if (!step.outputs.empty()) {
                plan.globalInputBufferIndex = step.outputs.front().bufferIndex;
            }
            continue;
        }
        if (step.kind == NodeKind::VoiceOutput) {
            for (const auto& input : step.inputs) {
                if (input.destPortIndex == 0 && input.sourceBufferIndex >= 0) {
                    plan.voiceMixBufferIndices.push_back(input.sourceBufferIndex);
                }
            }
        }
    }
}

}
