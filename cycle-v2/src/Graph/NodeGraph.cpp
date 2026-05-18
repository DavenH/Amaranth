#include "NodeGraph.h"

namespace CycleV2 {

namespace {

Port input(String id, String label, PortDomain domain, ChannelLayout layout = ChannelLayout::Mono) {
    return { std::move(id), std::move(label), domain, layout, true };
}

Port output(String id, String label, PortDomain domain, ChannelLayout layout = ChannelLayout::Mono) {
    return { std::move(id), std::move(label), domain, layout, false };
}

Node node(String id, String title, String subtitle, Rectangle<float> bounds,
          std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        std::move(id),
        std::move(title),
        std::move(subtitle),
        bounds,
        std::move(inputs),
        std::move(outputs)
    };
}

}

NodeGraph NodeGraph::createDemoGraph() {
    NodeGraph graph;

    graph.nodes.push_back(node(
            "voice",
            "Voice Context",
            "6 voices · detune · pan · phase",
            { 80.f, 95.f, 250.f, 175.f },
            {},
            {
                    output("pitch", "Pitch", PortDomain::PitchSignal),
                    output("voice", "Voice", PortDomain::VoiceControlSignal)
            }));

    graph.nodes.push_back(node(
            "wave",
            "Trilinear Wave Surface",
            "pitch-aware generator",
            { 410.f, 80.f, 310.f, 240.f },
            {
                    input("pitch", "Pitch", PortDomain::PitchSignal),
                    input("voice", "Voice", PortDomain::VoiceControlSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal)
            },
            {
                    output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    output("mesh", "Mesh", PortDomain::MeshField)
            }));

    graph.nodes.push_back(node(
            "fft",
            "FFT: 1 Cycle",
            "time -> mag + phase",
            { 810.f, 105.f, 220.f, 185.f },
            { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            }));

    graph.nodes.push_back(node(
            "mag",
            "Magnitude Sculpt",
            "layer 2 scratch target",
            { 1120.f, 45.f, 285.f, 180.f },
            {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal)
            },
            { output("mag", "Mag", PortDomain::SpectralMagnitudeSignal) }));

    graph.nodes.push_back(node(
            "phase",
            "Phase Sculpt",
            "phase mesh filter",
            { 1120.f, 275.f, 285.f, 180.f },
            { input("phase", "Phase", PortDomain::SpectralPhaseSignal) },
            { output("phase", "Phase", PortDomain::SpectralPhaseSignal) }));

    graph.nodes.push_back(node(
            "ifft",
            "IFFT",
            "cyclic mode",
            { 1495.f, 135.f, 230.f, 210.f },
            {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("phase", "Phase", PortDomain::SpectralPhaseSignal)
            },
            { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.nodes.push_back(node(
            "env",
            "Envelope",
            "volume curve",
            { 805.f, 440.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.nodes.push_back(node(
            "scratchEnv",
            "Envelope",
            "scratch attachment",
            { 430.f, 405.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.nodes.push_back(node(
            "multiply",
            "Multiply",
            "global volume",
            { 1810.f, 190.f, 235.f, 165.f },
            {
                    input("audio", "Audio", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    input("factor", "Factor", PortDomain::EnvelopeSignal)
            },
            { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.nodes.push_back(node(
            "out",
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

}
