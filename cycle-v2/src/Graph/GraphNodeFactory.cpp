#include "GraphNodeFactory.h"

namespace CycleV2 {

Node GraphNodeFactory::createNode(NodeKind kind, const String& id, Point<float> position) const {
    Node node;
    node.id = id;
    node.kind = kind;
    node.bounds = { position.x, position.y, 240.f, 170.f };

    switch (kind) {
        case NodeKind::VoiceContext:
            node.title = "Voice Context";
            node.subtitle = "pitch / voice";
            node.bounds.setSize(250.f, 175.f);
            node.outputs = {
                    output("pitch", "Pitch", PortDomain::PitchSignal),
                    output("voice", "Voice", PortDomain::VoiceControlSignal)
            };
            break;

        case NodeKind::TrilinearWaveSurface:
            node.title = "Trilinear Wave Surface";
            node.subtitle = "pitch-aware generator";
            node.bounds.setSize(310.f, 240.f);
            node.inputs = {
                    input("pitch", "Pitch", PortDomain::PitchSignal),
                    input("voice", "Voice", PortDomain::VoiceControlSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            };
            node.outputs = {
                    output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    output("mesh", "Mesh", PortDomain::MeshField)
            };
            break;

        case NodeKind::Fft:
            node.title = "FFT: 1 Cycle";
            node.subtitle = "time -> mag + phase";
            node.bounds.setSize(220.f, 185.f);
            node.inputs = { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            };
            break;

        case NodeKind::SpectralMagnitudeProcessor:
            node.title = "Magnitude Sculpt";
            node.subtitle = "mesh filter";
            node.bounds.setSize(285.f, 180.f);
            node.inputs = {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            };
            node.outputs = { output("mag", "Mag", PortDomain::SpectralMagnitudeSignal) };
            break;

        case NodeKind::SpectralPhaseProcessor:
            node.title = "Phase Sculpt";
            node.subtitle = "phase mesh filter";
            node.bounds.setSize(285.f, 180.f);
            node.inputs = { input("phase", "Phase", PortDomain::SpectralPhaseSignal) };
            node.outputs = { output("phase", "Phase", PortDomain::SpectralPhaseSignal) };
            break;

        case NodeKind::Ifft:
            node.title = "IFFT";
            node.subtitle = "cyclic mode";
            node.bounds.setSize(230.f, 210.f);
            node.inputs = {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("phase", "Phase", PortDomain::SpectralPhaseSignal)
            };
            node.outputs = { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Envelope:
            node.title = "Envelope";
            node.subtitle = "control curve";
            node.bounds.setSize(235.f, 155.f);
            node.outputs = { output("env", "Env", PortDomain::EnvelopeSignal) };
            break;

        case NodeKind::Multiply:
            node.title = "Multiply";
            node.subtitle = "signal scale";
            node.bounds.setSize(235.f, 165.f);
            node.inputs = {
                    input("audio", "Audio", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    input("factor", "Factor", PortDomain::EnvelopeSignal)
            };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Output:
            node.title = "Output";
            node.subtitle = "stereo meters";
            node.bounds.setSize(210.f, 145.f);
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        default:
            node.title = "Processor";
            node.subtitle = "generic";
            node.inputs = { input("in", "In", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;
    }

    return node;
}

Port GraphNodeFactory::input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout,
        PortPurpose purpose) const {
    return { std::move(id), std::move(label), domain, layout, purpose, true };
}

Port GraphNodeFactory::output(String id, String label, PortDomain domain, ChannelLayout layout) const {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false };
}

}
