#pragma once

#include "NodeAudioProcessor.h"
#include "GraphRuntime.h"

#include <memory>
#include <unordered_map>

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
    struct ProcessorKey {
        String nodeId;
        int voiceIndex {};

        bool operator==(const ProcessorKey& other) const {
            return nodeId == other.nodeId && voiceIndex == other.voiceIndex;
        }
    };

    struct ProcessorKeyHash {
        size_t operator()(const ProcessorKey& key) const {
            const size_t nodeHash = (size_t) key.nodeId.hashCode64();
            const size_t voiceHash = std::hash<int> {}(key.voiceIndex);
            return nodeHash ^ (voiceHash + 0x9e3779b9 + (nodeHash << 6) + (nodeHash >> 2));
        }
    };

    struct PreparationSignature {
        uint64_t revision {};
        String configurationKey;
        size_t maximumFrameCount {};
        double sampleRate {};
        PortDomain domain { PortDomain::ControlSignal };
        ChannelLayout channelLayout { ChannelLayout::Mono };
        double bpm {};
        int beatsPerMeasure {};

        bool operator==(const PreparationSignature& other) const {
            return revision == other.revision
                    && configurationKey == other.configurationKey
                    && maximumFrameCount == other.maximumFrameCount
                    && sampleRate == other.sampleRate
                    && domain == other.domain
                    && channelLayout == other.channelLayout
                    && bpm == other.bpm
                    && beatsPerMeasure == other.beatsPerMeasure;
        }
    };

    struct CachedProcessor {
        AudioModuleRole role { AudioModuleRole::None };
        std::unique_ptr<NodeAudioProcessor> processor;
        PreparationSignature preparation;
        size_t preparationCount {};
        bool prepared {};
    };

    struct PreparedVoice {
        int voiceIndex {};
        const GraphExecutionPlan* plan {};
        std::vector<NodeAudioProcessor*> processors;
    };

    CachedProcessor& processorFor(
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
    mutable std::unordered_map<ProcessorKey, CachedProcessor, ProcessorKeyHash> processors;
    mutable std::unordered_map<int, PreparedVoice> preparedVoices;
};

}
