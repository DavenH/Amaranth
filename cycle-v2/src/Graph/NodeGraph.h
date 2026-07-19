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
    TrilinearMesh,
    Fft,
    Ifft,
    Envelope,
    Add,
    Multiply,
    GuideCurve,
    ImpulseResponse,
    Waveshaper,
    Reverb,
    Delay,
    Equalizer,
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

enum class MorphDimension {
    Yellow,
    Red,
    Blue
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

struct NodeParameter {
    String id;
    String label;
    String value;
};

struct Node {
    String id;
    NodeKind kind { NodeKind::GenericProcessor };
    String title;
    String subtitle;
    Rectangle<float> bounds;
    std::vector<NodeParameter> parameters;
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

struct SignalProbe {
    String id;
    String sourceNodeId;
    String sourcePortId;
    String anchorDestNodeId;
    String anchorDestPortId;
    String label;
    float tapPosition { 0.5f };
    int railOrder {};
};

struct NodeNaturalSize {
    float width {};
    float height {};
};

class NodeGraph {
public:
    const std::vector<Node>& getNodes() const { return nodes; }
    const std::vector<Edge>& getEdges() const { return edges; }
    const std::vector<SignalProbe>& getSignalProbes() const { return signalProbes; }
    uint64_t getRevision() const { return revision; }

    const Node* findNode(const String& nodeId) const;
    Node* findNodeForEditing(const String& nodeId);

    void addNode(Node node);
    void addEdge(Edge edge);
    void addSignalProbe(SignalProbe probe);
    bool removeSignalProbe(const String& probeId);
    SignalProbe* findSignalProbeForEditing(const String& probeId);
    const SignalProbe* findSignalProbe(const String& probeId) const;
    const SignalProbe* findSignalProbeForSource(
            const String& sourceNodeId,
            const String& sourcePortId) const;
    void removeNode(const String& nodeId);
    void removeEdgeAt(size_t index);
    void removeEdgesToInput(const String& nodeId, const String& portId);
    bool replaceNodeParameters(const String& nodeId, std::vector<NodeParameter> parameters);
    bool setNodeBounds(const String& nodeId, Rectangle<float> bounds);
    void translateNodes(const std::vector<String>& nodeIds, Point<float> offset);
    void markChanged() { ++revision; }

    static NodeGraph createDemoGraph();

private:
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<SignalProbe> signalProbes;
    uint64_t revision {};
};

Colour colourForDomain(PortDomain domain);
Colour colourForMorphDimension(MorphDimension dimension);
String labelForDomain(PortDomain domain);
String labelForChannelLayout(ChannelLayout layout);
String labelForNodeKind(NodeKind kind);
String parameterValueForNode(const Node& node, const String& parameterId, const String& fallback = {});
NodeNaturalSize naturalSizeForNode(const Node& node);

}
