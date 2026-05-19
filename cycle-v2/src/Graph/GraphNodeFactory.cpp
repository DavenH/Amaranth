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
            node.subtitle = "waveform start";
            node.parameters = {
                    { "domain", "Start Domain", "waveform" },
                    { "voices", "Voices", "1" }
            };
            node.outputs = {
                    output("context", "Context", PortDomain::DomainContext)
            };
            break;

        case NodeKind::WaveSource:
            node.title = "Wave";
            node.subtitle = "time source";
            node.inputs = { input("context", "Context", PortDomain::DomainContext) };
            node.outputs = { output("out", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::ImageSource:
            node.title = "Image";
            node.subtitle = "raster source";
            node.inputs = { input("context", "Context", PortDomain::DomainContext) };
            node.outputs = { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::TrilinearWaveSurface:
            node.title = "Trilinear Wave Surface";
            node.subtitle = "pitch-aware generator";
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

        case NodeKind::TrilinearMesh:
            node.title = "Trilinear Mesh";
            node.subtitle = "mesh operand";
            node.inputs = {
                    input("context", "Context", PortDomain::DomainContext),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            };
            node.outputs = {
                    output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo)
            };
            break;

        case NodeKind::Fft:
            node.title = "FFT: 1 Cycle";
            node.subtitle = "time -> mag + phase";
            node.parameters = {
                    { "cycleFrames", "Cycle Frames", "2048" }
            };
            node.inputs = { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            };
            break;

        case NodeKind::SpectralMagnitudeProcessor:
            node.title = "Magnitude Sculpt";
            node.subtitle = "mesh filter";
            node.inputs = {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            };
            node.outputs = { output("mag", "Mag", PortDomain::SpectralMagnitudeSignal) };
            break;

        case NodeKind::SpectralPhaseProcessor:
            node.title = "Phase Sculpt";
            node.subtitle = "phase mesh filter";
            node.inputs = { input("phase", "Phase", PortDomain::SpectralPhaseSignal) };
            node.outputs = { output("phase", "Phase", PortDomain::SpectralPhaseSignal) };
            break;

        case NodeKind::Ifft:
            node.title = "IFFT";
            node.subtitle = "cyclic mode";
            node.parameters = {
                    { "cycleFrames", "Cycle Frames", "2048" },
                    { "mode", "Mode", "cyclic" }
            };
            node.inputs = {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("phase", "Phase", PortDomain::SpectralPhaseSignal)
            };
            node.outputs = { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Envelope:
            node.title = "Envelope";
            node.subtitle = "control curve";
            node.outputs = { output("env", "Env", PortDomain::EnvelopeSignal) };
            break;

        case NodeKind::Add:
            node.title = "Add";
            node.subtitle = "combine";
            node.inputs = {
                    input("left", "A", PortDomain::ControlSignal),
                    input("right", "B", PortDomain::ControlSignal)
            };
            node.outputs = { output("out", "Out", PortDomain::ControlSignal) };
            break;

        case NodeKind::Multiply:
            node.title = "Multiply";
            node.subtitle = "operation";
            node.inputs = {
                    input("left", "A", PortDomain::ControlSignal),
                    input("right", "B", PortDomain::ControlSignal)
            };
            node.outputs = { output("out", "Out", PortDomain::ControlSignal) };
            break;

        case NodeKind::GuideCurve:
            node.title = "Guide";
            node.subtitle = "mesh attachment";
            node.outputs = { output("guide", "Guide", PortDomain::EnvelopeSignal) };
            break;

        case NodeKind::ImpulseResponse:
            node.title = "IR";
            node.subtitle = "convolution";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Waveshaper:
            node.title = "Waveshaper";
            node.subtitle = "transfer curve";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Reverb:
            node.title = "Reverb";
            node.subtitle = "space";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Delay:
            node.title = "Delay";
            node.subtitle = "echo";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::StereoSplit:
            node.title = "Stereo Split";
            node.subtitle = "L/R breakout";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = {
                    output("left", "Left", PortDomain::TimeSignal, ChannelLayout::Left),
                    output("right", "Right", PortDomain::TimeSignal, ChannelLayout::Right)
            };
            break;

        case NodeKind::StereoJoin:
            node.title = "Stereo Join";
            node.subtitle = "L/R combine";
            node.inputs = {
                    input("left", "Left", PortDomain::TimeSignal, ChannelLayout::Left),
                    input("right", "Right", PortDomain::TimeSignal, ChannelLayout::Right)
            };
            node.outputs = { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        case NodeKind::Output:
            node.title = "Output";
            node.subtitle = "sink";
            node.inputs = { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;

        default:
            node.title = "Processor";
            node.subtitle = "generic";
            node.inputs = { input("in", "In", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            node.outputs = { output("out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) };
            break;
    }

    const auto naturalSize = naturalSizeForNode(node);
    node.bounds.setSize(naturalSize.width, naturalSize.height);
    return node;
}

Port GraphNodeFactory::input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout,
        PortPurpose purpose,
        PortSide side) const {
    return { std::move(id), std::move(label), domain, layout, purpose, true, side };
}

Port GraphNodeFactory::output(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout,
        PortSide side) const {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false, side };
}

}
