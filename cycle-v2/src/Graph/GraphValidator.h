#pragma once

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
};

class GraphValidator {
public:
    std::vector<GraphValidationIssue> validate(const NodeGraph& graph) const;
    bool isValid(const NodeGraph& graph) const;

private:
    bool isVoiceAwareDestination(const Port& port) const;
    bool concreteOperationDomain(PortDomain domain) const;
    PortDomain domainFromContextInput(const NodeGraph& graph, const Node& node) const;
    PortDomain firstResolvedInputDomain(const NodeGraph& graph, const String& nodeId, int depth) const;
    PortDomain resolvedEdgeDomain(const NodeGraph& graph, const Edge& edge, int depth = 0) const;
    void validateOperationInputs(const NodeGraph& graph, std::vector<GraphValidationIssue>& issues) const;
    bool domainsCompatible(const Port& source, const Port& dest) const;
    bool channelLayoutsCompatible(const Port& source, const Port& dest) const;
};

}
