#pragma once

#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;

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
    MixedOperationDomains,
    MissingRequiredNode,
    DuplicateSingletonNode,
    ProcessingScopeMismatch,
    ConflictingProcessingScope,
    GlobalNodeUnreachable,
    GlobalNodeCannotReachOutput,
    AmbiguousVoiceOutput,
    MissingVoiceContextAssignment,
    MultipleActiveVoiceContexts
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
    std::vector<GraphValidationIssue> validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges) const;
    static bool acceptsProposedIssues(
            const std::vector<GraphValidationIssue>& before,
            const std::vector<GraphValidationIssue>& after);
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
            const GraphAudioScopeAnalysis* scopeAnalysis,
            EdgeIssueReporter& reporter) const;
    bool isVoiceAwareDestination(const Port& port) const;
    void validateOperationInputs(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
            std::vector<GraphValidationIssue>& issues) const;
    void validateAudioScopes(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphAudioScopeAnalysis& analysis,
            std::vector<GraphValidationIssue>& issues) const;
    void validateVoiceContextAssignments(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            std::vector<GraphValidationIssue>& issues) const;
    bool domainsCompatible(const Port& source, const Port& dest) const;
    bool channelLayoutsCompatible(const Port& source, const Port& dest) const;

    GraphDomainResolver domainResolver;
};

}
