#include "GraphValidator.h"

namespace CycleV2 {

namespace {

const Node* findNode(const NodeGraph& graph, const String& id) {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

const Port* findPort(const Node& node, const String& id, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == id) {
            return &port;
        }
    }

    return nullptr;
}

void addIssue(
        std::vector<GraphValidationIssue>& issues,
        GraphValidationCode code,
        const String& message) {
    issues.push_back({ code, message });
}

}

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    std::vector<GraphValidationIssue> issues;

    for (const auto& edge : graph.getEdges()) {
        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        const Node* destNode = findNode(graph, edge.destNodeId);

        if (sourceNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourceNode,
                     "Missing source node: " + edge.sourceNodeId);
            continue;
        }

        if (destNode == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationNode,
                     "Missing destination node: " + edge.destNodeId);
            continue;
        }

        const Port* source = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* dest = findPort(*destNode, edge.destPortId, true);

        if (source == nullptr) {
            addIssue(issues, GraphValidationCode::MissingSourcePort,
                     "Missing source port: " + edge.sourceNodeId + "." + edge.sourcePortId);
            continue;
        }

        if (dest == nullptr) {
            addIssue(issues, GraphValidationCode::MissingDestinationPort,
                     "Missing destination port: " + edge.destNodeId + "." + edge.destPortId);
            continue;
        }

        if (edge.attachment) {
            if (source->domain != PortDomain::EnvelopeSignal) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentSource,
                         "Attachments currently require an envelope source: " + edge.sourceNodeId);
            }

            if (dest->purpose != PortPurpose::ScratchAttachment) {
                addIssue(issues, GraphValidationCode::InvalidAttachmentDestination,
                         "Attachment destination is not a scratch port: " + edge.destNodeId + "." + dest->id);
            }

            continue;
        }

        if (dest->purpose == PortPurpose::ScratchAttachment) {
            addIssue(issues, GraphValidationCode::ScratchPortRequiresAttachment,
                     "Scratch ports require ProcessingAttachment routing: " + edge.destNodeId + "." + dest->id);
            continue;
        }

        if (!domainsCompatible(*source, *dest)) {
            addIssue(issues, GraphValidationCode::DomainMismatch,
                     "Domain mismatch: " + labelForDomain(source->domain) + " -> " + labelForDomain(dest->domain));
        }

        if (source->domain == PortDomain::PitchSignal && !isVoiceAwareDestination(*dest)) {
            addIssue(issues, GraphValidationCode::PitchRequiresVoiceAwareDestination,
                     "Pitch can only feed voice-aware generators or processors: " + edge.destNodeId + "." + dest->id);
        }
    }

    return issues;
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::isVoiceAwareDestination(const Port& port) const {
    return port.domain == PortDomain::PitchSignal || port.domain == PortDomain::VoiceControlSignal;
}

bool GraphValidator::domainsCompatible(const Port& source, const Port& dest) const {
    return source.domain == dest.domain;
}

}
