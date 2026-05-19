#include "NodeGraph.h"

#include <algorithm>

namespace CycleV2 {

namespace {

Port input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortPurpose purpose = PortPurpose::Signal,
        PortSide side = PortSide::Left) {
    return { std::move(id), std::move(label), domain, layout, purpose, true, side };
}

Port output(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortSide side = PortSide::Right) {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false, side };
}

Node node(String id, NodeKind kind, String title, String subtitle, Rectangle<float> bounds,
          std::vector<Port> inputs, std::vector<Port> outputs) {
    Node result {
        std::move(id),
        kind,
        std::move(title),
        std::move(subtitle),
        bounds,
        std::move(inputs),
        std::move(outputs)
    };
    const auto naturalSize = naturalSizeForNode(result);
    result.bounds.setSize(naturalSize.width, naturalSize.height);
    return result;
}

NodeNaturalSize minimumPreviewSizeForKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::WaveformStart:                return { 0.f, 0.f };
        case NodeKind::SpectralStart:                return { 0.f, 0.f };
        case NodeKind::WaveSource:                  return { 220.f, 90.f };
        case NodeKind::TrilinearWaveSurface:         return { 300.f, 150.f };
        case NodeKind::TrilinearMesh:                return { 260.f, 130.f };
        case NodeKind::VoiceContext:                 return { 230.f, 105.f };
        case NodeKind::Fft:                          return { 0.f, 0.f };
        case NodeKind::SpectralMagnitudeProcessor:   return { 240.f, 95.f };
        case NodeKind::SpectralPhaseProcessor:       return { 240.f, 95.f };
        case NodeKind::Ifft:                         return { 0.f, 0.f };
        case NodeKind::Envelope:                     return { 220.f, 85.f };
        case NodeKind::Add:                          return { 58.f, 44.f };
        case NodeKind::Multiply:                     return { 58.f, 44.f };
        case NodeKind::GuideCurve:                   return { 220.f, 85.f };
        case NodeKind::ImpulseResponse:              return { 210.f, 72.f };
        case NodeKind::Waveshaper:                   return { 190.f, 80.f };
        case NodeKind::Reverb:                       return { 0.f, 0.f };
        case NodeKind::Delay:                        return { 0.f, 0.f };
        case NodeKind::StereoSplit:                  return { 0.f, 0.f };
        case NodeKind::StereoJoin:                   return { 0.f, 0.f };
        case NodeKind::Output:                       return { 0.f, 0.f };
        default:                                     return { 190.f, 76.f };
    }
}

}

void NodeGraph::addNode(Node nodeToAdd) {
    nodes.push_back(std::move(nodeToAdd));
}

void NodeGraph::addEdge(Edge edgeToAdd) {
    edges.push_back(std::move(edgeToAdd));
}

void NodeGraph::removeNode(const String& nodeId) {
    nodes.erase(
            std::remove_if(
                    nodes.begin(),
                    nodes.end(),
                    [&](const Node& node) {
                        return node.id == nodeId;
                    }),
            nodes.end());

    edges.erase(
            std::remove_if(
                    edges.begin(),
                    edges.end(),
                    [&](const Edge& edge) {
                        return edge.sourceNodeId == nodeId || edge.destNodeId == nodeId;
                    }),
            edges.end());
}

void NodeGraph::removeEdgeAt(size_t index) {
    if (index >= edges.size()) {
        return;
    }

    edges.erase(edges.begin() + (int) index);
}

void NodeGraph::removeEdgesToInput(const String& nodeId, const String& portId) {
    edges.erase(
            std::remove_if(
                    edges.begin(),
                    edges.end(),
                    [&](const Edge& edge) {
                        return edge.destNodeId == nodeId && edge.destPortId == portId;
                    }),
            edges.end());
}

NodeGraph NodeGraph::createDemoGraph() {
    NodeGraph graph;

    graph.addNode(node(
            "voice",
            NodeKind::VoiceContext,
            "Voice Context",
            "6 voices / detune / pan / phase",
            { 80.f, 90.f, 300.f, 220.f },
            {},
            {
                    output("pitch", "Pitch", PortDomain::PitchSignal),
                    output("voice", "Voice", PortDomain::VoiceControlSignal)
            }));

    graph.addNode(node(
            "waveform",
            NodeKind::WaveformStart,
            "Waveform Start",
            "time domain",
            { 470.f, 80.f, 260.f, 155.f },
            {
                    input("pitch", "Pitch", PortDomain::PitchSignal),
                    input("voice", "Voice", PortDomain::VoiceControlSignal)
            },
            { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "waveMesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            "waveform operand",
            { 770.f, 80.f, 380.f, 280.f },
            {
                    input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
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
            { 1230.f, 160.f, 220.f, 185.f },
            { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            }));

    graph.addNode(node(
            "magMesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            "layer operand",
            { 1565.f, 20.f, 285.f, 180.f },
            {
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment)
            },
            { output("mesh", "Mesh", PortDomain::MeshField, ChannelLayout::Mono, PortSide::Bottom) }));

    graph.addNode(node(
            "addMag",
            NodeKind::Add,
            "Add",
            "magnitude layer",
            { 1618.f, 310.f, 180.f, 150.f },
            {
                    input("signal", "Signal", PortDomain::SpectralMagnitudeSignal),
                    input("operand", "Mesh", PortDomain::MeshField, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Top)
            },
            { output("out", "Out", PortDomain::SpectralMagnitudeSignal) }));

    graph.addNode(node(
            "phaseMesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            "phase operand",
            { 1565.f, 680.f, 285.f, 180.f },
            {},
            { output("mesh", "Mesh", PortDomain::MeshField, ChannelLayout::Mono, PortSide::Top) }));

    graph.addNode(node(
            "addPhase",
            NodeKind::Add,
            "Add",
            "phase layer",
            { 1618.f, 520.f, 180.f, 150.f },
            {
                    input("signal", "Signal", PortDomain::SpectralPhaseSignal),
                    input("operand", "Mesh", PortDomain::MeshField, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Bottom)
            },
            { output("out", "Out", PortDomain::SpectralPhaseSignal) }));

    graph.addNode(node(
            "ifft",
            NodeKind::Ifft,
            "IFFT",
            "cyclic mode",
            { 1965.f, 400.f, 230.f, 210.f },
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
            { 1270.f, 810.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.addNode(node(
            "scratchEnv",
            NodeKind::Envelope,
            "Envelope",
            "scratch attachment",
            { 500.f, 505.f, 235.f, 155.f },
            {},
            { output("env", "Env", PortDomain::EnvelopeSignal) }));

    graph.addNode(node(
            "multiply",
            NodeKind::Multiply,
            "Multiply",
            "global volume",
            { 2290.f, 555.f, 235.f, 165.f },
            {
                    input("audio", "Audio", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    input("factor", "Factor", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Bottom)
            },
            { output("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "out",
            NodeKind::Output,
            "Output",
            "stereo meters",
            { 2650.f, 565.f, 210.f, 145.f },
            { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {}));

    graph.edges = {
            { "voice", "pitch", "waveform", "pitch", PortDomain::PitchSignal, false },
            { "voice", "voice", "waveform", "voice", PortDomain::VoiceControlSignal, false },
            { "waveform", "time", "waveMesh", "time", PortDomain::TimeSignal, false },
            { "scratchEnv", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal, true },
            { "scratchEnv", "env", "magMesh", "scratch", PortDomain::EnvelopeSignal, true },
            { "waveMesh", "time", "fft", "time", PortDomain::TimeSignal, false },
            { "fft", "mag", "addMag", "signal", PortDomain::SpectralMagnitudeSignal, false },
            { "magMesh", "mesh", "addMag", "operand", PortDomain::MeshField, false },
            { "fft", "phase", "addPhase", "signal", PortDomain::SpectralPhaseSignal, false },
            { "phaseMesh", "mesh", "addPhase", "operand", PortDomain::MeshField, false },
            { "addMag", "out", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false },
            { "addPhase", "out", "ifft", "phase", PortDomain::SpectralPhaseSignal, false },
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
        case PortDomain::ControlSignal:           return "Universal";
        default:                                  return "Unknown";
    }
}

String labelForChannelLayout(ChannelLayout layout) {
    switch (layout) {
        case ChannelLayout::Mono:         return "";
        case ChannelLayout::LinkedStereo: return "L/R";
        case ChannelLayout::Left:         return "L";
        case ChannelLayout::Right:        return "R";
        case ChannelLayout::StereoPair:   return "Pair";
        default:                          return "?";
    }
}

String labelForNodeKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::GenericProcessor:             return "Generic Processor";
        case NodeKind::VoiceContext:                 return "Voice Context";
        case NodeKind::WaveformStart:                return "Waveform Start";
        case NodeKind::SpectralStart:                return "Spectral Start";
        case NodeKind::WaveSource:                   return "Wave Source";
        case NodeKind::TrilinearWaveSurface:         return "Trilinear Wave Surface";
        case NodeKind::TrilinearMesh:                return "Trilinear Mesh";
        case NodeKind::Fft:                          return "FFT";
        case NodeKind::SpectralMagnitudeProcessor:   return "Spectral Magnitude Processor";
        case NodeKind::SpectralPhaseProcessor:       return "Spectral Phase Processor";
        case NodeKind::Ifft:                         return "IFFT";
        case NodeKind::Envelope:                     return "Envelope";
        case NodeKind::Add:                          return "Add";
        case NodeKind::Multiply:                     return "Multiply";
        case NodeKind::GuideCurve:                   return "Guide Curve";
        case NodeKind::ImpulseResponse:              return "Impulse Response";
        case NodeKind::Waveshaper:                   return "Waveshaper";
        case NodeKind::Reverb:                       return "Reverb";
        case NodeKind::Delay:                        return "Delay";
        case NodeKind::StereoSplit:                  return "Stereo Split";
        case NodeKind::StereoJoin:                   return "Stereo Join";
        case NodeKind::Output:                       return "Output";
        default:                                     return "Unknown";
    }
}

NodeNaturalSize naturalSizeForNode(const Node& node) {
    const int portRows = jmax((int) node.inputs.size(), (int) node.outputs.size());
    const auto preview = minimumPreviewSizeForKind(node.kind);

    if (node.kind == NodeKind::Add || node.kind == NodeKind::Multiply) {
        return { 150.f, 118.f };
    }

    const float titleWidth = (float) node.title.length() * 8.5f;
    const float subtitleWidth = (float) node.subtitle.length() * 6.0f;

    const float headerWidth = titleWidth + subtitleWidth + 72.f;
    const float portWidth = 120.f;
    const float previewWidth = preview.width + 26.f;
    const float width = jmax(headerWidth, portWidth, previewWidth);

    const float headerHeight = 42.f;
    const float portHeight = 16.f + (float) portRows * 34.f;
    const float previewHeight = preview.height + 26.f;
    const float height = headerHeight + portHeight + previewHeight;

    return { width, height };
}

}
