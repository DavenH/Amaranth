#pragma once

#include "GraphDomainResolver.h"
#include "GraphValidator.h"
#include "../Runtime/NodeDspConfiguration.h"
#include "../Runtime/NodeModuleRegistry.h"

#include <unordered_map>
#include <vector>

namespace CycleV2 {

enum class GraphCompileCode {
    CycleDetected
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
};

struct CompiledSignalProbe {
    String probeId;
    int sourceStepIndex { -1 };
    int sourceOutputIndex { -1 };
};

struct GraphExecutionStep {
    String nodeId;
    NodeKind kind { NodeKind::GenericProcessor };
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
    PublishedNodeConfiguration configuration;
    std::vector<GraphStepInput> inputs;
    std::vector<GraphStepOutput> outputs;
    std::vector<GraphStepAttachment> attachments;
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

private:
    struct ConfigurationEntry {
        String nodeId;
        NodeConfigurationPublisher publisher;
    };

    void publishConfigurations(std::vector<GraphExecutionStep>& steps) const;

    GraphDomainResolver domainResolver;
    GraphValidator validator;
    NodeModuleRegistry moduleRegistry;
    NodeDspConfigurationFactory configurationFactory;
    mutable std::vector<ConfigurationEntry> configurations;
};

}
