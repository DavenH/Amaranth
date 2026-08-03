#include "NodePortVisualResolver.h"

namespace CycleV2 {

PortVisualSemantic NodePortVisualResolver::semanticFor(const Port& port) {
    if (!port.input) {
        return PortVisualSemantic::None;
    }
    if (port.attachmentType == AttachmentType::ModulationTriple) {
        return PortVisualSemantic::ModulationYrb;
    }
    if (port.attachmentType == AttachmentType::Unison) {
        return PortVisualSemantic::UnisonConfiguration;
    }
    if (port.purpose == PortPurpose::ScratchAttachment) {
        return PortVisualSemantic::ScratchAttachment;
    }
    if (port.domain == PortDomain::PitchSignal) {
        return PortVisualSemantic::PitchEnvelope;
    }
    if (port.domain == PortDomain::DomainContext) {
        return PortVisualSemantic::VoiceContext;
    }

    return PortVisualSemantic::None;
}

PortVisualSemantic NodePortVisualResolver::modulationSemantic(bool includesYellow) {
    return includesYellow
            ? PortVisualSemantic::ModulationYrb
            : PortVisualSemantic::ModulationRb;
}

Colour NodePortVisualResolver::colourFor(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:
        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
            return colourForDomain(domain);
        default:
            return colourForDomain(PortDomain::ControlSignal);
    }
}

}
