#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

using namespace juce;

enum class PortDomain {
    DomainContext,
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
    WaveSource,
    ImageSource,
    TrilinearWaveSurface,
    TrilinearMesh,
    Fft,
    SpectralMagnitudeProcessor,
    SpectralPhaseProcessor,
    Ifft,
    Envelope,
    Add,
    Multiply,
    GuideCurve,
    ImpulseResponse,
    Waveshaper,
    Reverb,
    Delay,
    StereoSplit,
    StereoJoin,
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

enum class PortSide {
    Left,
    Right,
    Top,
    Bottom
};

struct Port {
    String id;
    String label;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
    PortPurpose purpose { PortPurpose::Signal };
    bool input {};
    PortSide side { PortSide::Left };
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

struct NodeNaturalSize {
    float width {};
    float height {};
};

class NodeGraph {
public:
    const std::vector<Node>& getNodes() const { return nodes; }
    const std::vector<Edge>& getEdges() const { return edges; }
    std::vector<Node>& getNodesForEditing() { return nodes; }
    std::vector<Edge>& getEdgesForEditing() { return edges; }

    void addNode(Node node);
    void addEdge(Edge edge);
    void removeNode(const String& nodeId);
    void removeEdgeAt(size_t index);
    void removeEdgesToInput(const String& nodeId, const String& portId);

    static NodeGraph createDemoGraph();

private:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

Colour colourForDomain(PortDomain domain);
String labelForDomain(PortDomain domain);
String labelForChannelLayout(ChannelLayout layout);
String labelForNodeKind(NodeKind kind);
NodeNaturalSize naturalSizeForNode(const Node& node);

}
