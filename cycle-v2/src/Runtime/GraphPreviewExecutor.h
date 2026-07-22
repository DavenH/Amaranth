#pragma once

#include "NodePreviewProcessor.h"
#include "GraphAudioExecutor.h"
#include "../Graph/GraphCompiler.h"

namespace CycleV2 {

struct NodePreviewResult {
    String nodeId;
    PreviewModuleRole role { PreviewModuleRole::None };
    std::vector<float> primary;
    std::vector<float> secondary;
    size_t gridColumns {};
    size_t gridRows {};
    PortDomain domain { PortDomain::TimeSignal };
};

struct GraphPreviewResult {
    struct SignalProbePreview {
        String probeId;
        std::vector<float> values;
        size_t gridColumns {};
        size_t gridRows {};
        PortDomain domain { PortDomain::TimeSignal };
        ChannelLayout channelLayout { ChannelLayout::Mono };
        bool connected {};
    };

    std::vector<NodePreviewResult> nodes;
    std::vector<int> previewResultIndexByStep;
    std::vector<SignalProbePreview> probes;
    size_t indexedNodeCount {};
    size_t addressLookupCount {};
    size_t aliasedInputCount {};
    size_t reusedCapturedTraversalCount {};
    size_t renderedNodeCount {};
};

class GraphPreviewExecutor {
public:
    GraphPreviewResult render(const GraphExecutionPlan& plan, size_t pointCount) const;
    GraphPreviewResult render(
            const GraphExecutionPlan& plan,
            const GraphAudioResult& audioResult,
            size_t pointCount) const;
    GraphPreviewResult render(
            const GraphExecutionPlan& plan,
            const GraphAudioResult& audioResult,
            const std::vector<SignalProbe>& probes,
            size_t pointCount) const;
    GraphPreviewResult render(
            const GraphExecutionPlan& plan,
            const GraphAudioResultView& audioResult,
            const std::vector<SignalProbe>& probes,
            size_t pointCount) const;
    void renderIncremental(
            const GraphExecutionPlan& plan,
            const GraphAudioResultView& audioResult,
            const std::vector<SignalProbe>& probes,
            const std::vector<String>& dirtyNodeIds,
            size_t pointCount,
            GraphPreviewResult& result) const;
    void renderIncremental(
            const GraphExecutionPlan& plan,
            const GraphAudioResultView& audioResult,
            const std::vector<SignalProbe>& probes,
            const std::vector<uint8_t>& dirtyNodes,
            size_t pointCount,
            GraphPreviewResult& result) const;
};

}
