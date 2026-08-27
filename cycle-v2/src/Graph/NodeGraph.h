#pragma once

#include <JuceHeader.h>

#include <memory>
#include <optional>
#include <unordered_map>
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
    ModulationSource,
    ModulationTriple,
    WaveSource,
    ImageSource,
    TrilinearMesh,
    SpectralLayer,
    Fft,
    Ifft,
    Envelope,
    Add,
    Multiply,
    ImpulseResponse,
    Waveshaper,
    Unison,
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

enum class ConnectionKind {
    Signal,
    ConfigurationAttachment,
    ProcessingAttachment
};

enum class AttachmentType {
    None,
    ScratchEnvelope,
    ModulationTriple,
    Unison
};

enum class GuideCurveField {
    Time,
    Red,
    Blue,
    Phase,
    Amplitude,
    Curve
};

struct TrimeshCubeComponentGuideTarget {
    int cubeIndex { -1 };
    GuideCurveField field { GuideCurveField::Time };

    bool operator==(const TrimeshCubeComponentGuideTarget& other) const {
        return cubeIndex == other.cubeIndex && field == other.field;
    }
};

struct GuideCurveAssignment {
    String guideId;
    String targetNodeId;
    TrimeshCubeComponentGuideTarget target;

    bool targets(const String& nodeId, const TrimeshCubeComponentGuideTarget& candidate) const {
        return targetNodeId == nodeId && target == candidate;
    }
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

enum class DefaultModulationSlot {
    None,
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
    ConnectionKind connectionKind { ConnectionKind::Signal };
    AttachmentType attachmentType { AttachmentType::None };
    DefaultModulationSlot defaultModulationSlot { DefaultModulationSlot::None };
};

struct NodeParameter {
    String id;
    String label;
    String value;
};

class NodeModelState {
public:
    virtual ~NodeModelState() = default;

    virtual String schemaId() const = 0;
    virtual int schemaVersion() const = 0;
    virtual uint64_t revision() const = 0;
    virtual var writeJSON() const = 0;
    virtual bool equals(const NodeModelState& other) const = 0;
};

using NodeModelStatePtr = std::shared_ptr<const NodeModelState>;

struct GuideCurveResource {
    String id;
    String shortLabel;
    String name;
    int colourIndex {};
    int shelfOrder {};
    bool enabled { true };
    float noise {};
    float dcOffset {};
    float phase {};
    NodeModelStatePtr model;
};

struct AudioSampleResource {
    String id;
    String name;
    double sampleRate {};
    std::vector<float> samples;
};

struct NodeAudioResourceBinding {
    String nodeId;
    String resourceId;
    String mode;
};

struct NodeAudioResourceSummary {
    String name;
    String mode;
    int sampleCount {};
};

struct Node {
    String id;
    NodeKind kind { NodeKind::GenericProcessor };
    String subtitle;
    Rectangle<float> bounds;
    std::vector<NodeParameter> parameters;
    std::vector<Port> inputs;
    std::vector<Port> outputs;
    NodeModelStatePtr model;
    var editorState;
};

struct Edge {
    String sourceNodeId;
    String sourcePortId;
    String destNodeId;
    String destPortId;
    PortDomain domain {};
    ConnectionKind connectionKind { ConnectionKind::Signal };
    AttachmentType attachmentType { AttachmentType::None };

    Edge() = default;
    Edge(
            String sourceNode,
            String sourcePort,
            String destinationNode,
            String destinationPort,
            PortDomain edgeDomain,
            ConnectionKind kind = ConnectionKind::Signal,
            AttachmentType type = AttachmentType::None)
        : sourceNodeId(std::move(sourceNode)),
          sourcePortId(std::move(sourcePort)),
          destNodeId(std::move(destinationNode)),
          destPortId(std::move(destinationPort)),
          domain(edgeDomain),
          connectionKind(kind),
          attachmentType(type) {}

    // Transitional source compatibility for graph fixtures. Serialized and
    // compiled graph state always uses the typed representation above.
    Edge(
            String sourceNode,
            String sourcePort,
            String destinationNode,
            String destinationPort,
            PortDomain edgeDomain,
            bool processingAttachment)
        : Edge(
                std::move(sourceNode),
                std::move(sourcePort),
                std::move(destinationNode),
                std::move(destinationPort),
                edgeDomain,
                processingAttachment
                        ? ConnectionKind::ProcessingAttachment
                        : ConnectionKind::Signal,
                processingAttachment ? AttachmentType::ScratchEnvelope : AttachmentType::None) {}

    bool isAttachment() const { return connectionKind != ConnectionKind::Signal; }
    bool isConfigurationAttachment() const {
        return connectionKind == ConnectionKind::ConfigurationAttachment;
    }
    bool isProcessingAttachment() const {
        return connectionKind == ConnectionKind::ProcessingAttachment;
    }
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
    const std::vector<GuideCurveResource>& getGuideCurves() const { return guideCurves; }
    const std::vector<GuideCurveAssignment>& getGuideAssignments() const { return guideAssignments; }
    const std::vector<SignalProbe>& getSignalProbes() const { return signalProbes; }
    const std::vector<AudioSampleResource>& getAudioResources() const { return audioResources; }
    const std::vector<NodeAudioResourceBinding>& getAudioResourceBindings() const {
        return audioResourceBindings;
    }
    uint64_t getRevision() const { return revision; }

    const Node* findNode(const String& nodeId) const;
    Node* findNodeForEditing(const String& nodeId);

    void addNode(Node node);
    void addEdge(Edge edge);
    bool addGuideCurve(GuideCurveResource resource);
    bool removeGuideCurve(const String& guideId);
    GuideCurveResource* findGuideCurveForEditing(const String& guideId);
    const GuideCurveResource* findGuideCurve(const String& guideId) const;
    bool replaceGuideCurve(GuideCurveResource resource);
    bool moveGuideCurve(const String& guideId, int shelfOrder);
    bool assignGuideCurve(GuideCurveAssignment assignment);
    bool removeGuideAssignment(
            const String& nodeId,
            const TrimeshCubeComponentGuideTarget& target);
    int removeGuideAssignmentsOutsideCubeRange(
            const String& nodeId,
            int cubeCount);
    int guideUsageCount(const String& guideId) const;
    const std::vector<String>& guideTargetNodeIds(const String& guideId) const;
    const std::vector<String>& guideIdsForTargetNode(const String& nodeId) const;
    bool addAudioResource(AudioSampleResource resource);
    bool removeAudioResource(const String& resourceId);
    const AudioSampleResource* findAudioResource(const String& resourceId) const;
    bool bindAudioResource(NodeAudioResourceBinding binding);
    bool unbindAudioResource(const String& nodeId);
    const NodeAudioResourceBinding* findAudioResourceBinding(const String& nodeId) const;
    int audioResourceUsageCount(const String& resourceId) const;
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
    void removeEdgesFromOutput(const String& nodeId, const String& portId);
    bool replaceNodeParameters(const String& nodeId, std::vector<NodeParameter> parameters);
    bool replaceNodeModel(const String& nodeId, NodeModelStatePtr model);
    bool replaceNodeEditorState(const String& nodeId, var editorState);
    bool setNodeBounds(const String& nodeId, Rectangle<float> bounds);
    void translateNodes(const std::vector<String>& nodeIds, Point<float> offset);
    void markChanged() { ++revision; }

    static NodeGraph createDemoGraph();

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return (size_t) value.hashCode64();
        }
    };

    struct GuideTargetAddress {
        String nodeId;
        TrimeshCubeComponentGuideTarget target;

        bool operator==(const GuideTargetAddress& other) const {
            return nodeId == other.nodeId && target == other.target;
        }
    };

    struct GuideTargetAddressHash {
        size_t operator()(const GuideTargetAddress& value) const {
            size_t result = (size_t) value.nodeId.hashCode64();
            result ^= (size_t) value.target.cubeIndex + 0x9e3779b9U
                    + (result << 6U) + (result >> 2U);
            result ^= (size_t) value.target.field + 0x9e3779b9U
                    + (result << 6U) + (result >> 2U);
            return result;
        }
    };

    void rebuildGuideResourceIndex();
    void rebuildGuideAssignmentIndexes();

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<GuideCurveResource> guideCurves;
    std::vector<GuideCurveAssignment> guideAssignments;
    std::vector<SignalProbe> signalProbes;
    std::vector<AudioSampleResource> audioResources;
    std::vector<NodeAudioResourceBinding> audioResourceBindings;
    std::unordered_map<String, size_t, StringHash> guideResourceIndex;
    std::unordered_map<
            GuideTargetAddress,
            size_t,
            GuideTargetAddressHash> guideAssignmentTargetIndex;
    std::unordered_map<String, int, StringHash> guideUsageCounts;
    std::unordered_map<String, std::vector<String>, StringHash> guideTargetNodes;
    std::unordered_map<String, std::vector<String>, StringHash> targetNodeGuides;
    uint64_t revision {};
};

Colour colourForDomain(PortDomain domain);
Colour colourForMorphDimension(MorphDimension dimension);
String labelForDomain(PortDomain domain);
String labelForChannelLayout(ChannelLayout layout);
String labelForNodeKind(NodeKind kind);
String idForConnectionKind(ConnectionKind kind);
String idForAttachmentType(AttachmentType type);
std::optional<ConnectionKind> connectionKindForId(const String& id);
std::optional<AttachmentType> attachmentTypeForId(const String& id);
String parameterValueForNode(const Node& node, const String& parameterId, const String& fallback = {});
NodeNaturalSize naturalSizeForNode(const Node& node);

}
