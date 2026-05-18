#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

using namespace juce;

enum class PortDomain {
    TimeSignal,
    SpectralMagnitudeSignal,
    SpectralPhaseSignal,
    MeshField,
    EnvelopeSignal,
    PitchSignal,
    VoiceControlSignal,
    ControlSignal
};

enum class NodeKind {
    GenericProcessor,
    VoiceContext,
    TrilinearWaveSurface,
    Fft,
    SpectralMagnitudeProcessor,
    SpectralPhaseProcessor,
    Ifft,
    Envelope,
    Multiply,
    Output
};

enum class ChannelLayout {
    Mono,
    LinkedStereo,
    Left,
    Right,
    StereoPair
};

enum class PortPurpose {
    Signal,
    ScratchAttachment
};

struct Port {
    String id;
    String label;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
    PortPurpose purpose { PortPurpose::Signal };
    bool input {};
};

struct Node {
    String id;
    NodeKind kind { NodeKind::GenericProcessor };
    String title;
    String subtitle;
    Rectangle<float> bounds;
    std::vector<Port> inputs;
    std::vector<Port> outputs;
};

struct Edge {
    String sourceNodeId;
    String sourcePortId;
    String destNodeId;
    String destPortId;
    PortDomain domain {};
    bool attachment {};
};

class NodeGraph {
public:
    const std::vector<Node>& getNodes() const { return nodes; }
    const std::vector<Edge>& getEdges() const { return edges; }

    void addNode(Node node);
    void addEdge(Edge edge);

    static NodeGraph createDemoGraph();

private:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

Colour colourForDomain(PortDomain domain);
String labelForDomain(PortDomain domain);
String labelForNodeKind(NodeKind kind);

}
