#include "NodeGraph.h"

namespace CycleV2 {

namespace {

Port input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortPurpose purpose = PortPurpose::Signal) {
    return { std::move(id), std::move(label), domain, layout, purpose, true };
}

Port output(String id, String label, PortDomain domain, ChannelLayout layout = ChannelLayout::Mono) {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false };
}

Node node(String id, NodeKind kind, String title, String subtitle, Rectangle<float> bounds,
          std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        std::move(id),
        kind,
        std::move(title),
        std::move(subtitle),
        bounds,
        std::move(inputs),
        std::move(outputs)
    };
}

}

void NodeGraph::addNode(Node nodeToAdd) {
    nodes.push_back(std::move(nodeToAdd));
}

void NodeGraph::addEdge(Edge edgeToAdd) {
    edges.push_back(std::move(edgeToAdd));
}

NodeGraph NodeGraph::createDemoGraph() {
    NodeGraph graph;

    graph.addNode(node(
            "voice",
            NodeKind::VoiceContext,
            "Voice Context",
            "6 voices / detune / pan / phase",
            { 80.f, 95.f, 250.f, 175.f },
            {},
            {
                    output("pitch", "Pitch", PortDomain::PitchSignal),
                    output("voice", "Voice", PortDomain::VoiceControlSignal)
            }));

    graph.addNode(node(
            "wave",
            NodeKind::TrilinearWaveSurface,
            "Trilinear Wave Surface",
            "pitch-aware generator",
            { 410.f, 80.f, 310.f, 240.f },
            {
                    input("pitch", "Pitch", PortDomain::PitchSignal),
                    input("voice", "Voice", PortDomain::VoiceControlSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            },
            {
                    output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    output("mesh", "Mesh", PortDomain::MeshField)
            }));

    graph.addNode(node(
            "fft",
            NodeKind::Fft,
            "FFT: 1 Cycle",
            "time -> mag + phase",
            { 810.f, 105.f, 220.f, 185.f },
            { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            }));

    graph.addNode(node(
            "mag",
            NodeKind::SpectralMagnitudeProcessor,
            "Magnitude Sculpt",
            "layer 2 scratch target",
            { 1120.f, 45.f, 285.f, 180.f },
            {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            },
            { output("mag", "Mag", PortDomain::SpectralMagnitudeSignal) }));

    graph.addNode(node(
            "phase",
            NodeKind::SpectralPhaseProcessor,
            "Phase Sculpt",
            "phase mesh filter",
            { 1120.f, 275.f, 285.f, 180.f },
            { input("phase", "Phase", PortDomain::SpectralPhaseSignal) },
            { output("phase", "Phase", PortDomain::SpectralPhaseSignal) }));

    graph.addNode(node(
            "ifft",
            NodeKind::Ifft,
            "IFFT",
            "cyclic mode",
            { 1495.f, 135.f, 230.f, 210.f },
            {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("phase", "Phase", PortDomain::SpectralPhaseSignal)
            },
            { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "env",
            NodeKind::Envelope,
            "Envelope",
            "volume curve",
            { 805.f, 440.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.addNode(node(
            "scratchEnv",
            NodeKind::Envelope,
            "Envelope",
            "scratch attachment",
            { 430.f, 405.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.addNode(node(
            "multiply",
            NodeKind::Multiply,
            "Multiply",
            "global volume",
            { 1810.f, 190.f, 235.f, 165.f },
            {
                    input("audio", "Audio", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    input("factor", "Factor", PortDomain::EnvelopeSignal)
            },
            { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "out",
            NodeKind::Output,
            "Output",
            "stereo meters",
            { 2150.f, 205.f, 210.f, 145.f },
            { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {}));

    graph.edges = {
            { "voice", "pitch", "wave", "pitch", PortDomain::PitchSignal, false },
            { "voice", "voice", "wave", "voice", PortDomain::VoiceControlSignal, false },
            { "scratchEnv", "env", "wave", "scratch", PortDomain::EnvelopeSignal, true },
            { "wave", "time", "fft", "time", PortDomain::TimeSignal, false },
            { "fft", "mag", "mag", "mag", PortDomain::SpectralMagnitudeSignal, false },
            { "fft", "phase", "phase", "phase", PortDomain::SpectralPhaseSignal, false },
            { "mag", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false },
            { "phase", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, false },
            { "ifft", "time", "multiply", "audio", PortDomain::TimeSignal, false },
            { "env", "env", "multiply", "factor", PortDomain::EnvelopeSignal, false },
            { "multiply", "time", "out", "time", PortDomain::TimeSignal, false }
    };

    return graph;
}

Colour colourForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:              return Colour(0xff35d6d2);
        case PortDomain::SpectralMagnitudeSignal: return Colour(0xffffb347);
        case PortDomain::SpectralPhaseSignal:     return Colour(0xffb284ff);
        case PortDomain::MeshField:               return Colour(0xff7f95aa);
        case PortDomain::EnvelopeSignal:          return Colour(0xff67a7ff);
        case PortDomain::PitchSignal:             return Colour(0xffb8ff5c);
        case PortDomain::VoiceControlSignal:      return Colour(0xff74e28a);
        case PortDomain::ControlSignal:           return Colour(0xffc5cad3);
        default:                                  return Colour(0xffc5cad3);
    }
}

String labelForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:              return "Time";
        case PortDomain::SpectralMagnitudeSignal: return "Mag";
        case PortDomain::SpectralPhaseSignal:     return "Phase";
        case PortDomain::MeshField:               return "Mesh";
        case PortDomain::EnvelopeSignal:          return "Env";
        case PortDomain::PitchSignal:             return "Pitch";
        case PortDomain::VoiceControlSignal:      return "Voice";
        case PortDomain::ControlSignal:           return "Control";
        default:                                  return "Unknown";
    }
}

String labelForNodeKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::GenericProcessor:             return "Generic Processor";
        case NodeKind::VoiceContext:                 return "Voice Context";
        case NodeKind::TrilinearWaveSurface:         return "Trilinear Wave Surface";
        case NodeKind::Fft:                          return "FFT";
        case NodeKind::SpectralMagnitudeProcessor:   return "Spectral Magnitude Processor";
        case NodeKind::SpectralPhaseProcessor:       return "Spectral Phase Processor";
        case NodeKind::Ifft:                         return "IFFT";
        case NodeKind::Envelope:                     return "Envelope";
        case NodeKind::Multiply:                     return "Multiply";
        case NodeKind::Output:                       return "Output";
        default:                                     return "Unknown";
    }
}

}
