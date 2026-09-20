#include "Graph/DefaultOutputProbeResolver.h"

#include <algorithm>

namespace CycleV2 {

namespace {

bool isExcludedOutputEffect(NodeKind kind) {
    return kind == NodeKind::Reverb
            || kind == NodeKind::Delay
            || kind == NodeKind::Equalizer;
}

const Edge* uniqueSignalInput(const NodeGraph& graph, const String& nodeId) {
    const Edge* result {};
    for (const auto& edge : graph.getEdges()) {
        if (edge.destNodeId != nodeId || edge.isAttachment()) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &edge;
    }
    return result;
}

}

std::optional<DefaultOutputProbeAddress> DefaultOutputProbeResolver::resolve(
        const NodeGraph& graph) const {
    const Node* output {};
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::Output) {
            continue;
        }
        if (output != nullptr) {
            return std::nullopt;
        }
        output = &node;
    }
    if (output == nullptr) {
        return std::nullopt;
    }

    const Edge* input = uniqueSignalInput(graph, output->id);
    if (input == nullptr) {
        return std::nullopt;
    }
    const Node* source = graph.findNode(input->sourceNodeId);
    while (source != nullptr && isExcludedOutputEffect(source->kind)) {
        input = uniqueSignalInput(graph, source->id);
        if (input == nullptr) {
            return std::nullopt;
        }
        source = graph.findNode(input->sourceNodeId);
    }
    if (source == nullptr) {
        return std::nullopt;
    }
    return DefaultOutputProbeAddress { input->sourceNodeId, input->sourcePortId };
}

}
