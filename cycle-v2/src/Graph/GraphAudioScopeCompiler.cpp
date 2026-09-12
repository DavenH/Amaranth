#include "Graph/GraphAudioScopeCompiler.h"

#include <algorithm>

namespace CycleV2 {

namespace {

bool voiceOutputIsConsumed(
        const GraphExecutionPlan& plan,
        const GraphAudioScopeAnalysis& analysis,
        const String& nodeId,
        const String& portId) {
    return std::any_of(
            plan.signalEdges.begin(),
            plan.signalEdges.end(),
            [&](const Edge& edge) {
                return edge.sourceNodeId == nodeId
                        && edge.sourcePortId == portId
                        && analysis.scopeFor(edge.destNodeId) == AuthoredAudioScope::Voice;
            });
}

}

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
        if (analysis.scopeFor(step.nodeId) != AuthoredAudioScope::Voice) {
            continue;
        }
        for (const auto& output : step.outputs) {
            if (output.domain == PortDomain::TimeSignal
                    && output.channelLayout == ChannelLayout::LinkedStereo
                    && !voiceOutputIsConsumed(
                            plan,
                            analysis,
                            step.nodeId,
                            output.portId)) {
                plan.voiceMixBufferIndices.push_back(output.bufferIndex);
            }
        }
    }
}

}
