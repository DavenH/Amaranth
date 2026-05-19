#include "GraphCompiler.h"

#include <algorithm>

namespace CycleV2 {

namespace {

int indexOfNode(const std::vector<Node>& nodes, const String& nodeId) {
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[static_cast<size_t>(i)].id == nodeId) {
            return i;
        }
    }

    return -1;
}

const Node* findNode(const NodeGraph& graph, const String& nodeId) {
    const int index = indexOfNode(graph.getNodes(), nodeId);

    if (index < 0) {
        return nullptr;
    }

    return &graph.getNodes()[static_cast<size_t>(index)];
}

const Port* findPort(const Node& node, const String& portId, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

int inputPortIndex(const Node& node, const String& portId) {
    for (int i = 0; i < static_cast<int>(node.inputs.size()); ++i) {
        if (node.inputs[static_cast<size_t>(i)].id == portId) {
            return i;
        }
    }

    return -1;
}

PortDomain outputPortDomain(
        const std::vector<Edge>& resolvedEdges,
        const Node& node,
        const Port& port) {
    for (const auto& edge : resolvedEdges) {
        if (edge.sourceNodeId == node.id && edge.sourcePortId == port.id) {
            return edge.domain;
        }
    }

    return port.domain;
}

ChannelLayout outputPortChannelLayout(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const Node& node,
        const Port& port) {
    for (const auto& edge : resolvedEdges) {
        if (edge.sourceNodeId == node.id && edge.sourcePortId == port.id) {
            return domainResolver.resolvedChannelLayoutForEdge(graph, edge);
        }
    }

    return port.channelLayout;
}

std::vector<GraphStepOutput> buildStepOutputs(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const Node& node) {
    std::vector<GraphStepOutput> outputs;
    outputs.reserve(node.outputs.size());

    for (const auto& port : node.outputs) {
        outputs.push_back({
                port.id,
                outputPortDomain(resolvedEdges, node, port),
                outputPortChannelLayout(graph, resolvedEdges, domainResolver, node, port)
        });
    }

    return outputs;
}

int parameterInt(const Node& node, const String& parameterId, int fallback) {
    const String value = parameterValueForNode(node, parameterId);

    if (value.isEmpty()) {
        return fallback;
    }

    return value.getIntValue();
}

String transformModeForNode(const Node& node) {
    if (node.kind == NodeKind::Fft) {
        return parameterValueForNode(node, "window", "blackmanHarris");
    }

    if (node.kind == NodeKind::Ifft) {
        return parameterValueForNode(node, "mode", "cyclic");
    }

    return {};
}

int latencyCyclesForNode(const Node& node) {
    if (node.kind != NodeKind::Ifft) {
        return 0;
    }

    return parameterValueForNode(node, "mode", "cyclic") == "acyclicCarry" ? 1 : 0;
}

std::vector<String> buildNodeOrder(
        const NodeGraph& graph,
        std::vector<GraphCompileIssue>& issues) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();

    std::vector<int> indegrees(nodes.size(), 0);
    std::vector<bool> emitted(nodes.size(), false);
    std::vector<String> order;
    order.reserve(nodes.size());

    for (const auto& edge : edges) {
        const int sourceIndex = indexOfNode(nodes, edge.sourceNodeId);
        const int destIndex = indexOfNode(nodes, edge.destNodeId);

        if (sourceIndex >= 0 && destIndex >= 0 && sourceIndex != destIndex) {
            ++indegrees[static_cast<size_t>(destIndex)];
        }
    }

    while (order.size() < nodes.size()) {
        int readyIndex = -1;

        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            if (!emitted[static_cast<size_t>(i)] && indegrees[static_cast<size_t>(i)] == 0) {
                readyIndex = i;
                break;
            }
        }

        if (readyIndex < 0) {
            issues.push_back({
                    GraphCompileCode::CycleDetected,
                    "Graph contains a cycle in processing dependencies"
            });
            break;
        }

        emitted[static_cast<size_t>(readyIndex)] = true;
        order.push_back(nodes[static_cast<size_t>(readyIndex)].id);

        for (const auto& edge : edges) {
            if (edge.sourceNodeId != nodes[static_cast<size_t>(readyIndex)].id) {
                continue;
            }

            const int destIndex = indexOfNode(nodes, edge.destNodeId);
            if (destIndex >= 0 && destIndex != readyIndex) {
                --indegrees[static_cast<size_t>(destIndex)];
            }
        }
    }

    return order;
}

std::vector<GraphExecutionStep> buildExecutionSteps(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver,
        const NodeModuleRegistry& moduleRegistry) {
    std::vector<GraphExecutionStep> steps;
    steps.reserve(nodeOrder.size());

    for (const auto& nodeId : nodeOrder) {
        const int nodeIndex = indexOfNode(graph.getNodes(), nodeId);

        if (nodeIndex < 0) {
            continue;
        }

        const Node& node = graph.getNodes()[static_cast<size_t>(nodeIndex)];
        const auto descriptor = moduleRegistry.descriptorFor(node.kind);
        std::vector<GraphStepInput> inputs;

        for (const auto& edge : resolvedEdges) {
            if (edge.destNodeId != node.id) {
                continue;
            }

            inputs.push_back({
                    edge.sourceNodeId,
                    edge.sourcePortId,
                    edge.destPortId,
                    inputPortIndex(node, edge.destPortId),
                    edge.domain,
                    domainResolver.resolvedChannelLayoutForEdge(graph, edge)
            });
        }

        std::sort(
                inputs.begin(),
                inputs.end(),
                [&](const GraphStepInput& a, const GraphStepInput& b) {
                    return a.destPortIndex < b.destPortIndex;
                });

        steps.push_back({
                node.id,
                node.kind,
                descriptor.audioRole,
                descriptor.previewRole,
                descriptor.previewable,
                descriptor.cycle1AdapterBacked,
                descriptor.cycle1Reference,
                parameterInt(node, "cycleFrames", 2048),
                latencyCyclesForNode(node),
                transformModeForNode(node),
                node.parameters,
                std::move(inputs),
                buildStepOutputs(graph, resolvedEdges, domainResolver, node)
        });
    }

    return steps;
}

bool hasBufferForSource(
        const std::vector<GraphBufferPlan>& buffers,
        const String& sourceNodeId,
        const String& sourcePortId) {
    for (const auto& buffer : buffers) {
        if (buffer.sourceNodeId == sourceNodeId && buffer.sourcePortId == sourcePortId) {
            return true;
        }
    }

    return false;
}

std::vector<GraphBufferPlan> buildBufferPlan(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver) {
    std::vector<GraphBufferPlan> buffers;

    for (const auto& edge : resolvedEdges) {
        if (edge.domain == PortDomain::DomainContext) {
            continue;
        }

        if (hasBufferForSource(buffers, edge.sourceNodeId, edge.sourcePortId)) {
            continue;
        }

        buffers.push_back({
                edge.sourceNodeId + "." + edge.sourcePortId,
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.domain,
                domainResolver.resolvedChannelLayoutForEdge(graph, edge)
        });
    }

    return buffers;
}

}

bool GraphCompileResult::succeeded() const {
    return validationIssues.empty() && compileIssues.empty();
}

GraphCompileResult GraphCompiler::compile(const NodeGraph& graph) const {
    GraphCompileResult result;
    result.validationIssues = validator.validate(graph);

    if (!result.validationIssues.empty()) {
        return result;
    }

    result.plan.nodeOrder = buildNodeOrder(graph, result.compileIssues);

    if (result.compileIssues.empty()) {
        result.plan.signalEdges = domainResolver.resolveSignalEdges(graph, result.plan.nodeOrder);
        result.plan.buffers = buildBufferPlan(graph, result.plan.signalEdges, domainResolver);

        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment) {
                result.plan.attachments.push_back(edge);
            }
        }

        result.plan.steps = buildExecutionSteps(
                graph,
                result.plan.nodeOrder,
                result.plan.signalEdges,
                domainResolver,
                moduleRegistry);
    }

    if (!result.succeeded()) {
        result.plan = {};
    }

    return result;
}

}
