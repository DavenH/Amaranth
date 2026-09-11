#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

#include "Runtime/GraphRuntime.h"
#include "Runtime/NodeAudioProcessor.h"
#include "Runtime/PreparedOscillatorRegion.h"

namespace CycleV2 {

struct NodeAudioResult {
    String nodeId;
    SignalPayload output;
    std::vector<std::pair<String, SignalPayload>> outputs;
    std::vector<SignalTraversalGrid> probeTraversalGrids;
};

struct GraphAudioResult {
    SignalPayload output;
    std::vector<NodeAudioResult> nodes;
    bool cancelled {};
};

struct GraphAudioResultView {
    const SignalPayload* output {};
    std::vector<const NodeAudioResult*> nodes;
    bool cancelled {};
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
    using CancellationCheck = std::function<bool()>;
    void prepareExecution(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec,
            int voiceIndex = 0) const;
    void prepareRealtimeVoiceExecution(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec,
            int voiceIndex) const;
    void prepareRealtimeGlobalExecution(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec) const;
    size_t preparationCount(const String& nodeId, int voiceIndex = 0) const;
    size_t serviceNonRealtimePreparation() const;
    bool hasActiveVoiceTail(int voiceIndex) const;
    bool hasVoiceTailProcessor(int voiceIndex) const;
    size_t oscillatorFrameRenderCount(int voiceIndex) const;

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
    GraphAudioResultView processIncremental(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            const std::vector<String>& dirtyNodeIds,
            CancellationCheck cancellationCheck = {}) const;
    GraphAudioResultView processIncrementalIndexed(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            const std::vector<uint8_t>& dirtyNodes,
            CancellationCheck cancellationCheck = {}) const;
    GraphAudioResultView processIncrementalIndexed(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            size_t frameCount,
            const std::vector<uint8_t>& dirtyNodes,
            AudioVoiceContext voice,
            CancellationCheck cancellationCheck = {}) const;
    void clearIncrementalCache() const;
    void resetExecutionState() const;
    size_t diagnosticProcessCount(const String& nodeId) const;
    GraphAudioOutputView processRealtime(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            GraphProcessObserver* observer = nullptr) const;
    void beginRealtimeVoiceMix(
            const GraphExecutionPlan& plan,
            size_t frameCount) const;
    void processRealtimeVoiceToMix(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice) const;
    GraphAudioOutputView processRealtimeGlobal(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing) const;

private:
    enum class ProcessingPass {
        Complete,
        Voice,
        Global
    };

    struct ProcessorKey {
        String nodeId;
        int voiceIndex {};

        bool operator==(const ProcessorKey& other) const {
            return nodeId == other.nodeId && voiceIndex == other.voiceIndex;
        }
    };

    struct ProcessorKeyHash {
        size_t operator()(const ProcessorKey& key) const {
            const size_t nodeHash = static_cast<size_t>(key.nodeId.hashCode64());
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
        struct OscillatorRegion {
            int planRegionIndex { -1 };
            int materializationStepIndex { -1 };
            int midiNoteOffset {};
            std::vector<uint64_t> configurationRevisions;
            std::vector<float> pitchEnvelopeUnitValues;
            std::unique_ptr<PreparedOscillatorRegion> processor;
            uint64_t voiceSamplePosition {};
            bool active {};
        };

        int voiceIndex {};
        const GraphExecutionPlan* plan {};
        size_t maximumFrameCount {};
        double sampleRate {};
        std::vector<NodeAudioProcessor*> processors;
        std::vector<std::unique_ptr<OscillatorRegion>> oscillatorRegions;
        std::vector<OscillatorRegion*> oscillatorRegionByStep;
    };

    CachedProcessor& processorFor(
            const String& nodeId,
            int voiceIndex,
            AudioModuleRole role,
            const NodeAudioProcessorFactory& factory) const;
    void removeUnreferencedProcessors() const;
    static PreparedVoice::OscillatorRegion* oscillatorRegionForStep(
            PreparedVoice& voice,
            size_t stepIndex);
    static void renderOscillatorRegion(
            PreparedVoice::OscillatorRegion& region,
            const AudioVoiceContext& voice,
            AudioProcessTiming timing,
            const SignalPayload* signalBuffers,
            size_t signalBufferCount,
            size_t frameCount,
            SignalPayload& output);
    bool hasVoiceTailProcessor(int voiceIndex, bool activeOnly) const;
    GraphAudioResult processInternal(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            bool captureDiagnostics,
            GraphProcessObserver* observer,
            const std::vector<uint8_t>* dirtyNodes = nullptr,
            const CancellationCheck& cancellationCheck = {},
            GraphAudioResultView* incrementalResult = nullptr,
            ProcessingPass pass = ProcessingPass::Complete) const;
    void mixVoiceBoundary(
            const GraphExecutionPlan& plan,
            size_t frameCount) const;
    void loadMixedVoiceBoundary(
            const GraphExecutionPlan& plan,
            size_t frameCount) const;
    void prepareExecutionInternal(
            const GraphExecutionPlan& plan,
            const AudioExecutionSpec& spec,
            int voiceIndex,
            ProcessingPass pass) const;

    static constexpr int globalProcessorIndex = -1;

    mutable AudioProcessWorkArena workArena;
    mutable AudioProcessWorkArena voiceMixArena;
    mutable AudioProcessContext processContext;
    mutable std::vector<SignalPayload> bufferSlots;
    mutable std::vector<SignalPayload> voiceMixSlots;
    mutable const SignalPayload* realtimeOutput {};
    mutable std::unordered_map<ProcessorKey, CachedProcessor, ProcessorKeyHash> processors;
    mutable std::unordered_map<int, PreparedVoice> preparedVoices;
    mutable std::vector<String> diagnosticNodeIds;
    mutable std::vector<std::optional<NodeAudioResult>> diagnosticCache;
    mutable std::vector<size_t> diagnosticProcessCounts;
};

}
