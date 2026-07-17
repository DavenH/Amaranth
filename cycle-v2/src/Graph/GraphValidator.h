#pragma once

#include "GraphDomainResolver.h"
#include "NodeGraph.h"

#include <vector>

namespace CycleV2 {

enum class GraphValidationCode {
    MissingSourceNode,
    MissingDestinationNode,
    MissingSourcePort,
    MissingDestinationPort,
    DomainMismatch,
    ChannelLayoutMismatch,
    InvalidAttachmentSource,
    InvalidAttachmentDestination,
    ScratchPortRequiresAttachment,
    PitchRequiresVoiceAwareDestination,
    MixedOperationDomains
};

struct GraphValidationIssue {
    GraphValidationCode code {};
    String message;
    String sourceNodeId;
    String sourcePortId;
    String destNodeId;
    String destPortId;
};

class GraphValidator {
public:
    std::vector<GraphValidationIssue> validate(const NodeGraph& graph) const;
    bool isValid(const NodeGraph& graph) const;
    bool edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const;
    GraphValidationIssue validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const;
    PortDomain resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const;

private:
    class EdgeIssueReporter;

    void validateEdge(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain resolvedDomain,
            EdgeIssueReporter& reporter) const;
    bool isVoiceAwareDestination(const Port& port) const;
    void validateOperationInputs(
            const NodeGraph& graph,
            const GraphDomainResolution& resolution,
            std::vector<GraphValidationIssue>& issues) const;
    bool domainsCompatible(const Port& source, const Port& dest) const;
    bool channelLayoutsCompatible(const Port& source, const Port& dest) const;

    GraphDomainResolver domainResolver;
};

}
