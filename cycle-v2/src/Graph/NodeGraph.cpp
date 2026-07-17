#include "NodeGraph.h"

#include "GraphNodeFactory.h"
#include "NodeDefinition.h"

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

Node node(String id, NodeKind kind, String title, String subtitle, Point<float> position,
          std::vector<Port> inputs, std::vector<Port> outputs) {
    Node result {
        std::move(id),
        kind,
        std::move(title),
        std::move(subtitle),
        { position.x, position.y, 0.f, 0.f },
        {},
        std::move(inputs),
        std::move(outputs)
    };
    const auto naturalSize = naturalSizeForNode(result);
    result.bounds.setSize(naturalSize.width, naturalSize.height);
    return result;
}

template<typename Container, typename Predicate>
void eraseIf(Container& container, Predicate predicate) {
    container.erase(
            std::remove_if(
                    container.begin(),
                    container.end(),
                    predicate),
            container.end());
}

}

void NodeGraph::addNode(Node nodeToAdd) {
    if (findNode(nodeToAdd.id) != nullptr) {
        return;
    }
    nodes.push_back(std::move(nodeToAdd));
    ++revision;
}

void NodeGraph::addEdge(Edge edgeToAdd) {
    edges.push_back(std::move(edgeToAdd));
    ++revision;
}

void NodeGraph::removeNode(const String& nodeId) {
    const size_t previousNodeCount = nodes.size();
    eraseIf(nodes, [&](const Node& node) {
        return node.id == nodeId;
    });

    eraseIf(edges, [&](const Edge& edge) {
        return edge.sourceNodeId == nodeId || edge.destNodeId == nodeId;
    });
    if (nodes.size() != previousNodeCount) {
        ++revision;
    }
}

void NodeGraph::removeEdgeAt(size_t index) {
    if (index >= edges.size()) {
        return;
    }

    edges.erase(edges.begin() + (int) index);
    ++revision;
}

void NodeGraph::removeEdgesToInput(const String& nodeId, const String& portId) {
    const size_t previousEdgeCount = edges.size();
    eraseIf(edges, [&](const Edge& edge) {
        return edge.destNodeId == nodeId && edge.destPortId == portId;
    });
    if (edges.size() != previousEdgeCount) {
        ++revision;
    }
}

const Node* NodeGraph::findNode(const String& nodeId) const {
    for (const auto& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

Node* NodeGraph::findNodeForEditing(const String& nodeId) {
    for (auto& node : nodes) {
        if (node.id == nodeId) {
            return &node;
        }
    }
    return nullptr;
}

bool NodeGraph::replaceNodeParameters(const String& nodeId, std::vector<NodeParameter> parameters) {
    auto* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    node->parameters = std::move(parameters);
    ++revision;
    return true;
}

bool NodeGraph::setNodeBounds(const String& nodeId, Rectangle<float> bounds) {
    auto* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    node->bounds = bounds;
    ++revision;
    return true;
}

void NodeGraph::translateNodes(const std::vector<String>& nodeIds, Point<float> offset) {
    bool changed = false;
    for (auto& node : nodes) {
        if (std::find(nodeIds.begin(), nodeIds.end(), node.id) != nodeIds.end()) {
            node.bounds = node.bounds.translated(offset.x, offset.y);
            changed = true;
        }
    }
    if (changed) {
        ++revision;
    }
}

NodeGraph NodeGraph::createDemoGraph() {
    NodeGraph graph;

    graph.addNode(node(
            "voice",
            NodeKind::VoiceContext,
            "Voice Context",
            "waveform start / 6 voices",
            { 320.f, 420.f },
            {},
            {
                    output("context", "Context", PortDomain::DomainContext)
            }));
    graph.replaceNodeParameters("voice", {
            { "domain", "Start Domain", "waveform" },
            { "voices", "Voices", "6" },
            { "octave", "Octave", "0" },
            { "pitch", "Pitch", "0" },
            { "portamento", "Portamento", "0" },
            { "oversampling", "Oversampling", "1x" }
    });

    graph.addNode(node(
            "waveMesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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
            String::fromUTF8("Time → Freq"),
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
            "Trilinear Mesh",
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
            "Add",
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
            "Trilinear Mesh",
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
            "Add",
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
            String::fromUTF8("Freq → Time"),
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
    volumeEnvelope.subtitle = "volume curve";
    graph.addNode(std::move(volumeEnvelope));

    Node scratchEnvelope = nodeFactory.createNode(NodeKind::Envelope, "scratchEnv", { 320.f, 204.f });
    scratchEnvelope.subtitle = "scratch attachment";
    graph.addNode(std::move(scratchEnvelope));

    graph.addNode(node(
            "multiply",
            NodeKind::Multiply,
            "Multiply",
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
            "Output",
            "sink",
            { 2100.f, 420.f },
            { input("time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo) },
            {}));

    graph.edges = {
            { "voice", "context", "waveMesh", "context", PortDomain::DomainContext, false },
            { "scratchEnv", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal, true },
            { "scratchEnv", "env", "magMesh", "scratch", PortDomain::EnvelopeSignal, true },
            { "waveMesh", "out", "fft", "time", PortDomain::TimeSignal, false },
            { "fft", "mag", "addMag", "left", PortDomain::SpectralMagnitudeSignal, false },
            { "magMesh", "out", "addMag", "right", PortDomain::ControlSignal, false },
            { "fft", "phase", "addPhase", "left", PortDomain::SpectralPhaseSignal, false },
            { "phaseMesh", "out", "addPhase", "right", PortDomain::ControlSignal, false },
            { "addMag", "out", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false },
            { "addPhase", "out", "ifft", "phase", PortDomain::SpectralPhaseSignal, false },
            { "ifft", "time", "multiply", "left", PortDomain::TimeSignal, false },
            { "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, false },
            { "multiply", "out", "out", "time", PortDomain::TimeSignal, false }
    };

    return graph;
}

Colour colourForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::DomainContext:           return Colour(0xff72d49a);
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

Colour colourForMorphDimension(MorphDimension dimension) {
    switch (dimension) {
        case MorphDimension::Yellow: return Colour(0xffd7bf5f);
        case MorphDimension::Red:    return Colour(0xffd65a5a);
        case MorphDimension::Blue:   return Colour(0xff5f91e8);
    }

    return colourForDomain(PortDomain::ControlSignal);
}

String labelForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::DomainContext:           return "Context";
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
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    return definition != nullptr ? definition->displayName : "Unknown";
}

String parameterValueForNode(const Node& node, const String& parameterId, const String& fallback) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == parameterId) {
            return parameter.value;
        }
    }

    return fallback;
}

NodeNaturalSize naturalSizeForNode(const Node& node) {
    const int portRows = jmax((int) node.inputs.size(), (int) node.outputs.size());
    const auto* definition = NodeDefinitionRegistry::instance().find(node.kind);
    const auto preview = definition != nullptr
            ? definition->minimumPreviewSize
            : NodeNaturalSize { 190.f, 76.f };
    if (definition != nullptr && definition->fixedNaturalSize.width > 0.f) {
        return definition->fixedNaturalSize;
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
