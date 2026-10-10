#include "Graph/GraphConnectionValidator.h"

#include "Graph/GraphValidationContext.h"
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
    const GraphValidationContext context(graph);
    return validate(graph, context, first, second);
}

GraphConnectionValidation GraphConnectionValidator::validate(
        const NodeGraph& graph,
        const GraphValidationContext& context,
        const PortAddress& first,
        const PortAddress& second) const {
    GraphConnectionProposal proposal = propose(graph, first, second);
    GraphConnectionValidation result { std::move(proposal), {} };
    if (!result.succeeded()) {
        return result;
    }

    result.issues = context.validateProposal(
            graph,
            context.edgeIndex().edgesToInput(
                    result.destination.nodeId,
                    result.destination.portId),
            { result.edge });
    if (!result.issues.empty()
            && !GraphValidator::acceptsProposedIssues(
                    context.validationIssues(), result.issues)) {
        result.code = GraphEditCode::ValidationRejected;
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
