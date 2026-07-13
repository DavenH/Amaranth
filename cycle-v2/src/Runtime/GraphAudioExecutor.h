#pragma once

#include "NodeAudioProcessor.h"
#include "GraphRuntime.h"

#include <memory>

namespace CycleV2 {

struct NodeAudioResult {
    String nodeId;
    SignalPayload output;
    std::vector<std::pair<String, SignalPayload>> outputs;
};

struct GraphAudioResult {
    SignalPayload output;
    std::vector<NodeAudioResult> nodes;
};

class GraphAudioExecutor {
public:
    void prepareExecution(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec,
            int voiceIndex = 0) const;
    size_t preparationCount(const String& nodeId, int voiceIndex = 0) const;

    GraphAudioResult process(const NodeGraph& graph, const GraphExecutionPlan& plan, size_t frameCount) const;
    GraphAudioResult process(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing) const;
    GraphAudioResult process(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            AudioVoiceContext voice) const;

private:
    struct CachedProcessor {
        String nodeId;
        int voiceIndex {};
        AudioModuleRole role { AudioModuleRole::None };
        std::unique_ptr<NodeAudioProcessor> processor;
        uint64_t preparedRevision {};
        size_t preparedFrameCount {};
        double preparedSampleRate {};
        size_t preparationCount {};
    };

    struct PortOutput {
        String nodeId;
        String portId;
        SignalPayload payload;
    };

    const SignalPayload* findOutputForNode(
            const std::vector<PortOutput>& outputs,
            const String& nodeId,
            const String& portId) const;
    bool isOutputNode(const NodeGraph& graph, const String& nodeId) const;
    NodeAudioProcessor* processorFor(
            const String& nodeId,
            int voiceIndex,
            AudioModuleRole role,
            const NodeAudioProcessorFactory& factory) const;
    void removeStaleProcessors(const GraphExecutionPlan& plan) const;

    mutable AudioProcessWorkArena workArena;
    mutable std::vector<CachedProcessor> processors;
};

}
