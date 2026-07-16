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

class GraphProcessObserver {
public:
    virtual ~GraphProcessObserver() = default;
    virtual void nodeProcessed(const String& nodeId, const AudioProcessContext& context) = 0;
};

struct GraphAudioOutputView {
    const SignalPayload* payload {};

    bool isValid() const { return payload != nullptr; }
};

class GraphAudioExecutor {
public:
    void prepareExecution(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec,
            int voiceIndex = 0) const;
    size_t preparationCount(const String& nodeId, int voiceIndex = 0) const;
    size_t serviceNonRealtimePreparation() const;

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
    GraphAudioOutputView processRealtime(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            GraphProcessObserver* observer = nullptr) const;

private:
    struct CachedProcessor {
        String nodeId;
        int voiceIndex {};
        AudioModuleRole role { AudioModuleRole::None };
        std::unique_ptr<NodeAudioProcessor> processor;
        uint64_t preparedRevision {};
        String preparedKey;
        size_t preparedFrameCount {};
        double preparedSampleRate {};
        PortDomain preparedDomain { PortDomain::ControlSignal };
        ChannelLayout preparedChannelLayout { ChannelLayout::Mono };
        double preparedBpm {};
        int preparedBeatsPerMeasure {};
        size_t preparationCount {};
    };

    struct PreparedVoice {
        int voiceIndex {};
        const GraphExecutionPlan* plan {};
        std::vector<NodeAudioProcessor*> processors;
    };

    NodeAudioProcessor* processorFor(
            const String& nodeId,
            int voiceIndex,
            AudioModuleRole role,
            const NodeAudioProcessorFactory& factory) const;
    void removeUnreferencedProcessors() const;
    GraphAudioResult processInternal(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            bool captureDiagnostics,
            GraphProcessObserver* observer) const;

    mutable AudioProcessWorkArena workArena;
    mutable AudioProcessContext processContext;
    mutable std::vector<SignalPayload> bufferSlots;
    mutable const SignalPayload* realtimeOutput {};
    mutable std::vector<CachedProcessor> processors;
    mutable std::vector<PreparedVoice> preparedVoices;
};

}
