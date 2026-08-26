#include "Nodes/Envelope/EnvelopePurpose.h"

#include <algorithm>

namespace CycleV2 {

EnvelopePurpose envelopePurposeFromString(const String& value) {
    if (value == "volume") {
        return EnvelopePurpose::Volume;
    }
    if (value == "pitch") {
        return EnvelopePurpose::Pitch;
    }
    if (value == "scratch") {
        return EnvelopePurpose::Scratch;
    }
    return EnvelopePurpose::Control;
}

String envelopePurposeToString(EnvelopePurpose purpose) {
    switch (purpose) {
        case EnvelopePurpose::Control: return "control";
        case EnvelopePurpose::Volume:  return "volume";
        case EnvelopePurpose::Pitch:   return "pitch";
        case EnvelopePurpose::Scratch: return "scratch";
    }
    return "control";
}

String envelopePurposeLabel(EnvelopePurpose purpose) {
    switch (purpose) {
        case EnvelopePurpose::Control: return "Control";
        case EnvelopePurpose::Volume:  return "Volume";
        case EnvelopePurpose::Pitch:   return "Pitch";
        case EnvelopePurpose::Scratch: return "Scratch";
    }
    return "Control";
}

PortDomain envelopeOutputDomain(EnvelopePurpose purpose) {
    switch (purpose) {
        case EnvelopePurpose::Control: return PortDomain::ControlSignal;
        case EnvelopePurpose::Volume:  return PortDomain::EnvelopeSignal;
        case EnvelopePurpose::Pitch:   return PortDomain::PitchSignal;
        case EnvelopePurpose::Scratch: return PortDomain::EnvelopeSignal;
    }
    return PortDomain::ControlSignal;
}

ConnectionKind envelopeConnectionKind(EnvelopePurpose purpose) {
    return purpose == EnvelopePurpose::Scratch
            ? ConnectionKind::ProcessingAttachment
            : ConnectionKind::Signal;
}

bool envelopePurposeAllowsLogarithmic(EnvelopePurpose purpose) {
    return purpose == EnvelopePurpose::Control || purpose == EnvelopePurpose::Volume;
}

EnvelopePurpose envelopePurposeFor(const Node& node) {
    return envelopePurposeFromString(parameterValueForNode(node, "purpose", "control"));
}

void applyEnvelopePurpose(Node& node) {
    if (node.kind != NodeKind::Envelope) {
        return;
    }

    node.parameters.erase(
            std::remove_if(
                    node.parameters.begin(),
                    node.parameters.end(),
                    [](const NodeParameter& parameter) {
                        return parameter.id == "dynamic";
                    }),
            node.parameters.end());
    if (node.outputs.empty()) {
        return;
    }

    const EnvelopePurpose purpose = envelopePurposeFor(node);
    Port& output = node.outputs.front();
    output.domain = envelopeOutputDomain(purpose);
    output.label = envelopePurposeLabel(purpose);
    output.connectionKind = envelopeConnectionKind(purpose);
    output.attachmentType = purpose == EnvelopePurpose::Scratch
            ? AttachmentType::ScratchEnvelope
            : AttachmentType::None;
    node.subtitle = envelopePurposeLabel(purpose).toLowerCase() + " envelope";

    if (envelopePurposeAllowsLogarithmic(purpose)) {
        return;
    }
    for (auto& parameter : node.parameters) {
        if (parameter.id == "logarithmic") {
            parameter.value = "0";
            return;
        }
    }
}

}
