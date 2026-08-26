#pragma once

#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphValidator.h"
#include "Runtime/NodeDspConfiguration.h"
#include "Runtime/NodeModuleRegistry.h"

#include <Audio/CycleDsp/UnisonCore.h>

#include <unordered_map>
#include <vector>

namespace CycleV2 {

enum class GraphCompileCode {
    CycleDetected,
    AmbiguousVoiceContext,
    UnsupportedReconstructionPolicy
};

enum class ExecutionCoordinate {
    Configuration,
    CycleField,
    SpectralFrame,
    SampleBlock
};

enum class RuntimeOwnershipScope {
    Context,
    SynthVoice,
    OscillatorRegion,
    UnisonLane
};

enum class OscillatorExecutionStrategy {
    ChainedPerLane,
    SharedSpectralFrame
};

enum class SpectralReconstructionPolicy {
    CyclicFrameCrossfade
};

enum class OscillatorTailPolicy {
    EnvelopeOwned
};

struct GraphCompileIssue {
    GraphCompileCode code {};
    String message;
};

struct GraphStepInput {
    String sourceNodeId;
    String sourcePortId;
    String destPortId;
    int destPortIndex {};
    int sourceBufferIndex { -1 };
    int sourceStepIndex { -1 };
    int sourceOutputIndex { -1 };
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct GraphStepOutput {
    String portId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
    int bufferIndex { -1 };
};

struct GraphStepAttachment {
    String sourceNodeId;
    String sourcePortId;
    String destPortId;
    PortDomain domain {};
    int sourceBufferIndex { -1 };
};

struct GraphBufferPlan {
    String id;
    String sourceNodeId;
    String sourcePortId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
    int firstProducerStep { -1 };
    int lastConsumerStep { -1 };
    DefaultModulationSlot defaultModulationSlot { DefaultModulationSlot::None };
    std::shared_ptr<const INodeDspConfiguration> defaultModulation;
};

struct GraphDependencyIndex {
    struct StringHash {
        size_t operator()(const String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    std::vector<String> nodeIds;
    std::vector<std::vector<int>> dependents;
    std::vector<std::vector<int>> dependencies;
    std::unordered_map<String, int, StringHash> nodeIndexById;
    std::unordered_map<String, int, StringHash> stepIndexById;
};

struct CompiledSignalProbe {
    String probeId;
    int sourceStepIndex { -1 };
    int sourceOutputIndex { -1 };
};

struct CompiledVoiceContext {
    String nodeId;
    String startDomain { "waveform" };
    int octave {};
    float pitchSemitones {};
    bool portamento {};
    int oversampling { 1 };
    std::shared_ptr<const INodeDspConfiguration> defaultModulation;
    std::shared_ptr<const INodeDspConfiguration> pitchEnvelope;
    std::vector<float> pitchEnvelopeUnitValues;
    std::shared_ptr<const INodeDspConfiguration> unison;
    CycleDsp::UnisonVoiceLayout lanes;
};

struct GraphExecutionStep {
    String nodeId;
    NodeKind kind { NodeKind::GenericProcessor };
    NodeExecutionTrait executionTrait { NodeExecutionTrait::SampleBlockProcessor };
    ExecutionCoordinate executionCoordinate { ExecutionCoordinate::SampleBlock };
    RuntimeOwnershipScope ownershipScope { RuntimeOwnershipScope::SynthVoice };
    int oscillatorRegionIndex { -1 };
    bool outputSink {};
    AudioModuleRole audioRole { AudioModuleRole::None };
    PreviewModuleRole previewRole { PreviewModuleRole::None };
    PreviewContract previewContract { PreviewContract::None };
    bool previewable {};
    bool cycle1AdapterBacked {};
    String cycle1Reference;
    int cycleFrames { 2048 };
    int latencyCycles {};
    String transformMode;
    std::vector<NodeParameter> parameters;
    NodeModelStatePtr model;
    PublishedNodeConfiguration configuration;
    std::vector<GraphStepInput> inputs;
    std::vector<GraphStepOutput> outputs;
    std::vector<GraphStepAttachment> attachments;
};

struct OscillatorRegionPlan {
    String id;
    String voiceContextNodeId;
    OscillatorExecutionStrategy strategy { OscillatorExecutionStrategy::ChainedPerLane };
    SpectralReconstructionPolicy reconstruction {
            SpectralReconstructionPolicy::CyclicFrameCrossfade };
    std::vector<int> stepIndices;
    int materializationStepIndex { -1 };
    int laneCount { 1 };
    int outputLatencySamples {};
    OscillatorTailPolicy tailPolicy { OscillatorTailPolicy::EnvelopeOwned };
    int outputTailSamples {};
};

struct GraphExecutionPlan {
    size_t maximumInputCount {};
    size_t maximumOutputCount { 1 };
    size_t maximumAttachmentCount {};
    size_t maximumTraversalColumns { 8 };
    std::vector<String> nodeOrder;
    std::vector<GraphExecutionStep> steps;
    std::vector<GraphBufferPlan> buffers;
    std::vector<Edge> signalEdges;
    std::vector<Edge> attachments;
    std::vector<Edge> configurationAttachments;
    std::vector<CompiledVoiceContext> voiceContexts;
    std::vector<OscillatorRegionPlan> oscillatorRegions;
    std::vector<CompiledSignalProbe> signalProbes;
    GraphDependencyIndex dependencyIndex;
};

struct GraphCompileResult {
    GraphExecutionPlan plan;
    std::vector<GraphValidationIssue> validationIssues;
    std::vector<GraphCompileIssue> compileIssues;

    bool succeeded() const;
};

class GraphCompiler {
public:
    GraphCompileResult compile(const NodeGraph& graph) const;
    void refreshSignalProbes(const NodeGraph& graph, GraphExecutionPlan& plan) const;

private:
    struct ConfigurationEntry {
        String nodeId;
        NodeConfigurationPublisher publisher;
    };

    void publishConfigurations(
            const NodeGraph& graph,
            std::vector<GraphExecutionStep>& steps) const;

    GraphDomainResolver domainResolver;
    GraphValidator validator;
    NodeModuleRegistry moduleRegistry;
    NodeDspConfigurationFactory configurationFactory;
    mutable std::vector<ConfigurationEntry> configurations;
};

}
