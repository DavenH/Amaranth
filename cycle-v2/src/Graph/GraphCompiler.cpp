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
                outputPortChannelLayout(graph, resolvedEdges, domainResolver, node, port),
                -1
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
        return parameterValueForNode(node, "mode", "cycle");
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
                    -1,
                    -1,
                    -1,
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
                node.kind == NodeKind::Output,
                descriptor.audioRole,
                descriptor.previewRole,
                descriptor.previewContract,
                descriptor.previewable,
                descriptor.cycle1AdapterBacked,
                descriptor.cycle1Reference,
                parameterInt(node, "cycleFrames", 2048),
                latencyCyclesForNode(node),
                transformModeForNode(node),
                node.parameters,
                {},
                std::move(inputs),
                buildStepOutputs(graph, resolvedEdges, domainResolver, node)
        });
    }

    return steps;
}

int bufferIndexFor(
        const std::vector<GraphBufferPlan>& buffers,
        const String& nodeId,
        const String& portId) {
    for (int i = 0; i < (int) buffers.size(); ++i) {
        if (buffers[(size_t) i].sourceNodeId == nodeId
                && buffers[(size_t) i].sourcePortId == portId) {
            return i;
        }
    }

    return -1;
}

int stepIndexFor(const GraphExecutionPlan& plan, const String& nodeId) {
    for (int i = 0; i < (int) plan.steps.size(); ++i) {
        if (plan.steps[(size_t) i].nodeId == nodeId) {
            return i;
        }
    }

    return -1;
}

int outputIndexFor(const GraphExecutionStep& step, const String& portId) {
    for (int i = 0; i < (int) step.outputs.size(); ++i) {
        if (step.outputs[(size_t) i].portId == portId) {
            return i;
        }
    }

    return -1;
}

void compileRouting(GraphExecutionPlan& plan) {
    for (int stepIndex = 0; stepIndex < (int) plan.steps.size(); ++stepIndex) {
        auto& step = plan.steps[(size_t) stepIndex];
        plan.maximumOutputCount = std::max(plan.maximumOutputCount, step.outputs.size());

        for (auto& input : step.inputs) {
            if (input.destPortIndex >= 0) {
                plan.maximumInputCount = std::max(
                        plan.maximumInputCount,
                        (size_t) input.destPortIndex + 1);
            }
            input.sourceBufferIndex = bufferIndexFor(
                    plan.buffers,
                    input.sourceNodeId,
                    input.sourcePortId);
            input.sourceStepIndex = stepIndexFor(plan, input.sourceNodeId);
            if (input.sourceStepIndex >= 0) {
                input.sourceOutputIndex = outputIndexFor(
                        plan.steps[(size_t) input.sourceStepIndex],
                        input.sourcePortId);
            }
            if (input.sourceBufferIndex >= 0) {
                plan.buffers[(size_t) input.sourceBufferIndex].lastConsumerStep = stepIndex;
            }
        }

        for (auto& output : step.outputs) {
            output.bufferIndex = bufferIndexFor(plan.buffers, step.nodeId, output.portId);
            if (output.bufferIndex >= 0) {
                auto& buffer = plan.buffers[(size_t) output.bufferIndex];
                buffer.firstProducerStep = stepIndex;
                buffer.lastConsumerStep = std::max(buffer.lastConsumerStep, stepIndex);
            }
        }

        for (const auto& attachment : plan.attachments) {
            if (attachment.destNodeId != step.nodeId) {
                continue;
            }

            const int sourceBufferIndex = bufferIndexFor(
                    plan.buffers,
                    attachment.sourceNodeId,
                    attachment.sourcePortId);
            step.attachments.push_back({
                    attachment.sourceNodeId,
                    attachment.sourcePortId,
                    attachment.destPortId,
                    attachment.domain,
                    sourceBufferIndex
            });
            if (sourceBufferIndex >= 0) {
                plan.buffers[(size_t) sourceBufferIndex].lastConsumerStep = stepIndex;
            }
        }
        plan.maximumAttachmentCount = std::max(
                plan.maximumAttachmentCount,
                step.attachments.size());
    }
}

int dependencyNodeIndex(const GraphDependencyIndex& index, const String& nodeId) {
    for (int i = 0; i < (int) index.nodeIds.size(); ++i) {
        if (index.nodeIds[(size_t) i] == nodeId) {
            return i;
        }
    }

    return -1;
}

void compileDependencyIndex(GraphExecutionPlan& plan) {
    auto& index = plan.dependencyIndex;
    index.nodeIds = plan.nodeOrder;
    index.dependents.clear();
    index.dependents.resize(index.nodeIds.size());

    const auto appendEdges = [&](const std::vector<Edge>& edges) {
        for (const auto& edge : edges) {
            const int source = dependencyNodeIndex(index, edge.sourceNodeId);
            const int destination = dependencyNodeIndex(index, edge.destNodeId);
            if (source < 0 || destination < 0) {
                continue;
            }

            auto& destinations = index.dependents[(size_t) source];
            if (std::find(destinations.begin(), destinations.end(), destination)
                    == destinations.end()) {
                destinations.push_back(destination);
            }
        }
    };

    appendEdges(plan.signalEdges);
    appendEdges(plan.attachments);
}

std::vector<GraphBufferPlan> buildBufferPlan(
        const NodeGraph& graph,
        const std::vector<Edge>& resolvedEdges,
        const GraphDomainResolver& domainResolver) {
    std::vector<GraphBufferPlan> buffers;

    for (const auto& node : graph.getNodes()) {
        for (const auto& port : node.outputs) {
            const PortDomain domain = outputPortDomain(resolvedEdges, node, port);
            if (domain == PortDomain::DomainContext) {
                continue;
            }

            buffers.push_back({
                    node.id + "." + port.id,
                    node.id,
                    port.id,
                    domain,
                    outputPortChannelLayout(graph, resolvedEdges, domainResolver, node, port)
            });
        }
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
        compileRouting(result.plan);
        compileDependencyIndex(result.plan);
        publishConfigurations(result.plan.steps);
    }

    if (!result.succeeded()) {
        result.plan = {};
    }

    return result;
}

void GraphCompiler::publishConfigurations(std::vector<GraphExecutionStep>& steps) const {
    const AudioExecutionSpec spec;

    for (auto& step : steps) {
        const String key = configurationFactory.keyFor(step.audioRole, step.parameters, spec);
        auto found = std::find_if(configurations.begin(), configurations.end(), [&](const auto& entry) {
            return entry.nodeId == step.nodeId;
        });

        if (found == configurations.end()) {
            configurations.push_back({ step.nodeId, {} });
            found = configurations.end() - 1;
        }

        step.configuration = found->publisher.publish(key, [&]() {
            return configurationFactory.create(step.audioRole, step.parameters, spec);
        });
    }
}

}
