#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>

#include "Runtime/GraphRuntime.h"
#include "Runtime/GraphAudioProcessorCache.h"
#include "Runtime/NodeAudioProcessor.h"
#include "Runtime/PreparedOscillatorRegion.h"

namespace CycleV2 {

struct ModulationSourceConfiguration;

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

struct GraphExecutionOperationCounts {
    uint32_t stepVisits {};
    uint32_t modulationBindingVisits {};
    uint32_t spectralTransferBindingVisits {};
    uint32_t contextPatches {};
    OscillatorRegionPerformanceCounts oscillator;
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
    size_t preparedBlockStorageValueCount() const;
    size_t preparedGridStorageValueCount() const;
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
            AudioVoiceContext voice,
            size_t traversalColumnCount = 0) const;
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
            GraphProcessObserver* observer = nullptr,
            GraphExecutionOperationCounts* operationCounts = nullptr) const;
    void beginRealtimeVoiceMix(
            const GraphExecutionPlan& plan,
            size_t frameCount) const;
    void processRealtimeVoiceToMix(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            GraphExecutionOperationCounts* operationCounts = nullptr) const;
    GraphAudioOutputView processRealtimeGlobal(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            GraphExecutionOperationCounts* operationCounts = nullptr) const;

private:
    enum class ProcessingPass {
        Complete,
        Voice,
        Global
    };

    struct CompleteDiagnosticExecution {
        size_t traversalColumnCount {};
    };

    struct IncrementalDiagnosticExecution {
        const std::vector<uint8_t>& dirtyNodes;
        const CancellationCheck& cancellationCheck;
        GraphAudioResultView& result;
    };

    struct RealtimeExecution {
        ProcessingPass pass { ProcessingPass::Complete };
        GraphProcessObserver* observer {};
        GraphExecutionOperationCounts* operationCounts {};
    };

    using ProcessingMode = std::variant<
            CompleteDiagnosticExecution,
            IncrementalDiagnosticExecution,
            RealtimeExecution>;

    struct PreparedVoice {
        struct ModulationBinding {
            size_t bufferIndex {};
            const ModulationSourceConfiguration* source {};
            int noteOffset {};
        };

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

        struct Step {
            AudioProcessContext context;
            uint32_t spectralTransferBindingCount {};
            bool hasBufferOutput {};
        };

        int voiceIndex {};
        const GraphExecutionPlan* plan {};
        size_t maximumFrameCount {};
        size_t traversalColumnCount {};
        double sampleRate {};
        std::vector<NodeAudioProcessor*> processors;
        std::vector<size_t> stepIndices;
        std::vector<NodeAudioProcessor*> tailProcessors;
        std::vector<ModulationBinding> modulationBindings;
        std::vector<Step> steps;
        std::vector<std::unique_ptr<OscillatorRegion>> oscillatorRegions;
        std::vector<OscillatorRegion*> oscillatorRegionByStep;
    };

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
            SignalPayload& output,
            OscillatorRegionPerformanceCounts* performanceCounts);
    bool hasVoiceTailProcessor(int voiceIndex, bool activeOnly) const;
    GraphAudioResult processInternal(
            const GraphExecutionPlan& plan,
            size_t frameCount,
            AudioProcessTiming timing,
            const AudioVoiceContext& voice,
            const ProcessingMode& mode) const;
    void mixVoiceBoundary(
            const GraphExecutionPlan& plan,
            size_t frameCount) const;
    void loadCompleteVoiceBoundary(
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
    void prepareStepContext(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step,
            PreparedVoice::Step& preparedStep) const;

    static constexpr int globalProcessorIndex = -1;

    mutable AudioProcessWorkArena workArena;
    mutable AudioProcessWorkArena voiceMixArena;
    mutable std::vector<SignalPayload> bufferSlots;
    mutable std::vector<SignalPayload> voiceMixSlots;
    mutable const SignalPayload* realtimeOutput {};
    mutable GraphAudioProcessorCache processorCache;
    mutable std::unordered_map<int, PreparedVoice> preparedVoices;
    mutable std::vector<String> diagnosticNodeIds;
    mutable std::vector<std::optional<NodeAudioResult>> diagnosticCache;
    mutable std::vector<size_t> diagnosticProcessCounts;
};

}
