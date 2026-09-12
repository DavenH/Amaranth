#include "Graph/NodeGraph.h"

#include <utility>

#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeDefinition.h"

#include "Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

namespace {

Port input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortPurpose purpose = PortPurpose::Signal,
        PortSide side = PortSide::Left,
        ConnectionKind connectionKind = ConnectionKind::Signal,
        AttachmentType attachmentType = AttachmentType::None,
        DefaultModulationSlot defaultSlot = DefaultModulationSlot::None) {
    return {
            std::move(id), std::move(label), domain, layout, purpose, true, side,
            purpose == PortPurpose::ScratchAttachment
                    ? ConnectionKind::ProcessingAttachment
                    : connectionKind,
            purpose == PortPurpose::ScratchAttachment
                    ? AttachmentType::ScratchEnvelope
                    : attachmentType,
            defaultSlot
    };
}

Port output(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout = ChannelLayout::Mono,
        PortSide side = PortSide::Right) {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false, side };
}

Node node(String id, NodeKind kind, String subtitle, Point<float> position,
          std::vector<Port> inputs, std::vector<Port> outputs) {
    Node result {
        std::move(id),
        kind,
        std::move(subtitle),
        { position.x, position.y, 0.f, 0.f },
        {},
        std::move(inputs),
        std::move(outputs)
    };
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    if (definition != nullptr && definition->modelCodec != nullptr) {
        result.model = definition->modelCodec->createDefault();
    }
    const auto naturalSize = naturalSizeForNode(result);
    result.bounds.setSize(naturalSize.width, naturalSize.height);
    return result;
}

}

NodeGraph NodeGraph::createDemoGraph() {
    NodeGraph graph;

    graph.addNode(node(
            "voice",
            NodeKind::VoiceContext,
            "waveform start",
            { 320.f, 420.f },
            {
                    input("modulation", "Modulation", PortDomain::VoiceControlSignal,
                            ChannelLayout::Mono, PortPurpose::Signal, PortSide::Left,
                            ConnectionKind::ConfigurationAttachment, AttachmentType::ModulationTriple),
                    input("pitch", "Pitch", PortDomain::PitchSignal),
                    input("unison", "Unison", PortDomain::VoiceControlSignal,
                            ChannelLayout::Mono, PortPurpose::Signal, PortSide::Left,
                            ConnectionKind::ConfigurationAttachment, AttachmentType::Unison)
            },
            {
                    output("context", "Context", PortDomain::DomainContext)
            }));
    graph.replaceNodeParameters("voice", {
            { "domain", "Start Domain", "waveform" },
            { "octave", "Octave", "0" },
            { "pitch", "Pitch", "0" },
            { "portamento", "Portamento", "0" },
            { "oversampling", "Oversampling", "1x" }
    });

    graph.addNode(node(
            "waveMesh",
            NodeKind::TrilinearMesh,
            "waveform operand",
            { 650.f, 420.f },
            {
                    input("context", "Context", PortDomain::DomainContext),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment),
                    input("yellow", "Yellow Morph", PortDomain::ControlSignal),
                    input("red", "Red Morph", PortDomain::ControlSignal),
                    input("blue", "Blue Morph", PortDomain::ControlSignal)
            },
            { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "fft",
            NodeKind::Fft,
            "cycle chunks",
            { 1080.f, 420.f },
            { input("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {
                    output("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    output("phase", "Phase", PortDomain::SpectralPhaseSignal)
            }));
    graph.replaceNodeParameters("fft", {
            { "cycleFrames", "Cycle Frames", "2048" },
            { "mode", "Mode", "cycle" }
    });

    graph.addNode(node(
            "magMesh",
            NodeKind::TrilinearMesh,
            "layer operand",
            { 1175.f, 170.f },
            {
                    input("context", "Context", PortDomain::DomainContext),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment),
                    input("yellow", "Yellow Morph", PortDomain::ControlSignal),
                    input("red", "Red Morph", PortDomain::ControlSignal),
                    input("blue", "Blue Morph", PortDomain::ControlSignal)
            },
            { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo, PortSide::Bottom) }));

    graph.addNode(node(
            "addMag",
            NodeKind::Add,
            "magnitude layer",
            { 1260.f, 420.f },
            {
                    input("left", "A", PortDomain::SpectralMagnitudeSignal),
                    input("right", "B", PortDomain::ControlSignal, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Top)
            },
            { output("out", "Out", PortDomain::SpectralMagnitudeSignal) }));

    graph.addNode(node(
            "phaseMesh",
            NodeKind::TrilinearMesh,
            "phase operand",
            { 1175.f, 760.f },
            {
                    input("context", "Context", PortDomain::DomainContext),
                    input("scratch", "Scratch", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::ScratchAttachment),
                    input("yellow", "Yellow Morph", PortDomain::ControlSignal),
                    input("red", "Red Morph", PortDomain::ControlSignal),
                    input("blue", "Blue Morph", PortDomain::ControlSignal)
            },
            { output("out", "Out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo, PortSide::Top) }));

    graph.addNode(node(
            "addPhase",
            NodeKind::Add,
            "phase layer",
            { 1260.f, 454.f },
            {
                    input("left", "A", PortDomain::SpectralPhaseSignal),
                    input("right", "B", PortDomain::ControlSignal, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Bottom)
            },
            { output("out", "Out", PortDomain::SpectralPhaseSignal) }));

    graph.addNode(node(
            "ifft",
            NodeKind::Ifft,
            "cyclic overlap",
            { 1600.f, 420.f },
            {
                    input("mag", "Mag", PortDomain::SpectralMagnitudeSignal),
                    input("phase", "Phase", PortDomain::SpectralPhaseSignal)
            },
            { output("time", "Time", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));
    graph.replaceNodeParameters("ifft", {
            { "cycleFrames", "Cycle Frames", "2048" },
            { "mode", "Mode", "cyclic" }
    });

    GraphNodeFactory nodeFactory;
    Node volumeEnvelope = nodeFactory.createNode(NodeKind::Envelope, "env", { 1660.f, 610.f });
    for (auto& parameter : volumeEnvelope.parameters) {
        if (parameter.id == "purpose") {
            parameter.value = "volume";
        }
    }
    applyEnvelopePurpose(volumeEnvelope);
    graph.addNode(std::move(volumeEnvelope));

    Node scratchEnvelope = nodeFactory.createNode(NodeKind::Envelope, "scratchEnv", { 320.f, 204.f });
    for (auto& parameter : scratchEnvelope.parameters) {
        if (parameter.id == "purpose") {
            parameter.value = "scratch";
        }
    }
    NodeDefinitionRegistry::instance().normalize(scratchEnvelope);
    graph.addNode(std::move(scratchEnvelope));

    graph.addNode(node(
            "multiply",
            NodeKind::Multiply,
            "global volume",
            { 1850.f, 420.f },
            {
                    input("left", "A", PortDomain::TimeSignal, ChannelLayout::LinkedStereo),
                    input("right", "B", PortDomain::EnvelopeSignal, ChannelLayout::Mono, PortPurpose::Signal, PortSide::Bottom)
            },
            { output("out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) }));

    graph.addNode(node(
            "out",
            NodeKind::Output,
            "sink",
            { 2100.f, 420.f },
            { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {}));

    graph.edges = {
            { "voice", "context", "waveMesh", "context", PortDomain::DomainContext, ConnectionKind::Signal },
            { "scratchEnv", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal,
                    ConnectionKind::ProcessingAttachment, AttachmentType::ScratchEnvelope },
            { "scratchEnv", "env", "magMesh", "scratch", PortDomain::EnvelopeSignal,
                    ConnectionKind::ProcessingAttachment, AttachmentType::ScratchEnvelope },
            { "waveMesh", "out", "fft", "time", PortDomain::TimeSignal, ConnectionKind::Signal },
            { "fft", "mag", "addMag", "left", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal },
            { "magMesh", "out", "addMag", "right", PortDomain::ControlSignal, ConnectionKind::Signal },
            { "fft", "phase", "addPhase", "left", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal },
            { "phaseMesh", "out", "addPhase", "right", PortDomain::ControlSignal, ConnectionKind::Signal },
            { "addMag", "out", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal },
            { "addPhase", "out", "ifft", "phase", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal },
            { "ifft", "time", "multiply", "left", PortDomain::TimeSignal, ConnectionKind::Signal },
            { "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal },
            { "multiply", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal }
    };

    return graph;
}

}
