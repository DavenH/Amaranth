#include "Graph/GraphConnectionValidator.h"

#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidator.h"

namespace CycleV2 {

namespace {

PortDomain edgeDomainForConnection(const Port& source, const Port& destination) {
    if (source.domain == PortDomain::ControlSignal
            && destination.domain != PortDomain::ControlSignal) {
        return destination.domain;
    }

    return source.domain;
}

}

GraphConnectionProposal GraphConnectionValidator::propose(
        const NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) const {
    if (first.input == second.input) {
        return { GraphEditCode::DirectionMismatch };
    }

    const PortAddress& sourceAddress = first.input ? second : first;
    const PortAddress& destinationAddress = first.input ? first : second;
    const Node* sourceNode = graph.findNode(sourceAddress.nodeId);
    const Node* destinationNode = graph.findNode(destinationAddress.nodeId);
    if (sourceNode == nullptr || destinationNode == nullptr) {
        return { GraphEditCode::MissingNode };
    }

    const Port* source = findPort(*sourceNode, sourceAddress.portId, false);
    const Port* destination = findPort(*destinationNode, destinationAddress.portId, true);
    if (source == nullptr || destination == nullptr) {
        return { GraphEditCode::MissingPort };
    }

    Edge edge {
            sourceAddress.nodeId,
            sourceAddress.portId,
            destinationAddress.nodeId,
            destinationAddress.portId,
            edgeDomainForConnection(*source, *destination),
            destination->purpose == PortPurpose::ScratchAttachment
                    ? ConnectionKind::ProcessingAttachment
                    : destination->connectionKind,
            destination->purpose == PortPurpose::ScratchAttachment
                    ? AttachmentType::ScratchEnvelope
                    : destination->attachmentType
    };
    return {
            GraphEditCode::Connected,
            std::move(edge),
            sourceAddress,
            destinationAddress
    };
}

GraphConnectionValidation GraphConnectionValidator::validate(
        const NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) const {
    GraphConnectionProposal proposal = propose(graph, first, second);
    GraphConnectionValidation result { std::move(proposal), {} };
    if (!result.succeeded()) {
        return result;
    }

    const GraphEdgeView proposedEdges(
            graph.getEdges(),
            edgeIndicesToInput(graph, result.destination),
            { result.edge });
    GraphValidator validator;
    result.issues = validator.validate(graph, proposedEdges);
    if (!result.issues.empty()
            && !GraphValidator::acceptsProposedIssues(
                    validator.validate(graph), result.issues)) {
        result.code = GraphEditCode::ValidationRejected;
    }
    return result;
}

std::vector<size_t> GraphConnectionValidator::edgeIndicesToInput(
        const NodeGraph& graph,
        const PortAddress& destination) const {
    std::vector<size_t> result;
    for (size_t index = 0; index < graph.getEdges().size(); ++index) {
        const Edge& edge = graph.getEdges()[index];
        if (edge.destNodeId == destination.nodeId
                && edge.destPortId == destination.portId) {
            result.push_back(index);
        }
    }
    return result;
}

const Port* GraphConnectionValidator::findPort(
        const Node& node,
        const String& portId,
        bool input) const {
    const auto& ports = input ? node.inputs : node.outputs;
    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }
    return nullptr;
}

}
