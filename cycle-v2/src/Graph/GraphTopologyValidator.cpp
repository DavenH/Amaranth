#include "Graph/GraphTopologyValidator.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidationTypes.h"
#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

namespace {

using NodeIdSet = std::unordered_set<
        String,
        GraphAudioScopeAnalysis::StringHash>;

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message) {
    issues.push_back({ code, message });
}

}

void GraphTopologyValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        std::vector<GraphValidationIssue>& issues) const {
    InteractionComplexityDiagnostics::recordValidationNodeVisits(
            graph.getNodes().size());
    InteractionComplexityDiagnostics::recordValidationEdgeVisits(edges.size());
    validateOperationInputs(graph, edges, resolution, issues);
    validateVoiceContextAssignments(graph, edges, issues);
}

void GraphTopologyValidator::validateOperationInputs(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::SpectralLayer) {
            const auto found = std::find_if(
                    edges.begin(),
                    edges.end(),
                    [&](const Edge& edge) {
                        return !edge.isAttachment() && edge.destNodeId == node.id;
                    });
            if (found != edges.end()) {
                const size_t edgeIndex = (size_t) std::distance(edges.begin(), found);
                const PortDomain domain = resolution.domains[edgeIndex];
                if (domain != PortDomain::TimeSignal
                        && domain != PortDomain::SpectralMagnitudeSignal
                        && domain != PortDomain::SpectralPhaseSignal) {
                    addIssue(
                            issues,
                            GraphValidationCode::DomainMismatch,
                            "Pan requires time, magnitude, or phase input: " + node.id);
                }
            }
            continue;
        }
        if (node.kind != NodeKind::Add && node.kind != NodeKind::Multiply) {
            continue;
        }

        PortDomain firstConcreteDomain {};
        bool hasConcreteDomain = false;
        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const Edge& edge = edges[edgeIndex];
            if (edge.isAttachment() || edge.destNodeId != node.id) {
                continue;
            }

            const PortDomain domain = resolution.domains[edgeIndex];
            if (!GraphDomainResolver::isConcreteOperationDomain(domain)) {
                continue;
            }
            if (node.kind == NodeKind::Multiply
                    && domain == PortDomain::SpectralPhaseSignal) {
                addIssue(
                        issues,
                        GraphValidationCode::DomainMismatch,
                        "Multiply cannot process spectral phase: " + node.id);
                break;
            }
            if (!hasConcreteDomain) {
                firstConcreteDomain = domain;
                hasConcreteDomain = true;
                continue;
            }
            if (domain != firstConcreteDomain) {
                addIssue(
                        issues,
                        GraphValidationCode::MixedOperationDomains,
                        "Operation node mixes incompatible domains: " + node.id);
                break;
            }
        }
    }
}

void GraphTopologyValidator::validateVoiceContextAssignments(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        std::vector<GraphValidationIssue>& issues) const {
    const int voiceContextCount = static_cast<int>(std::count_if(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::VoiceContext;
            }));
    if (voiceContextCount <= 1) {
        return;
    }

    std::unordered_map<String, String, GraphAudioScopeAnalysis::StringHash>
            explicitAssignments;
    for (const auto& edge : edges) {
        if (!edge.isAttachment() && edge.destPortId == "context") {
            explicitAssignments.emplace(edge.destNodeId, edge.sourceNodeId);
        }
    }

    NodeIdSet activeContexts;
    for (const auto& node : graph.getNodes()) {
        const bool acceptsContext = std::any_of(
                node.inputs.begin(),
                node.inputs.end(),
                [](const Port& port) {
                    return port.id == "context"
                            && port.domain == PortDomain::DomainContext;
                });
        if (!acceptsContext) {
            continue;
        }
        const auto assignment = explicitAssignments.find(node.id);
        if (assignment == explicitAssignments.end()) {
            addIssue(
                    issues,
                    GraphValidationCode::MissingVoiceContextAssignment,
                    "Multiple Voice Contexts require an explicit context for " + node.id);
            continue;
        }
        const Node* source = graph.findNode(assignment->second);
        if (source != nullptr && source->kind == NodeKind::VoiceContext) {
            activeContexts.emplace(source->id);
        }
    }

    if (activeContexts.size() > 1) {
        addIssue(
                issues,
                GraphValidationCode::MultipleActiveVoiceContexts,
                "Only one Voice Context can participate until multi-oscillator semantics are defined");
    }
}

}
