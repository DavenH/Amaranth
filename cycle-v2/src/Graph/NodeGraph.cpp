#include "NodeGraph.h"

#include "NodeParameterMap.h"

#include "GraphNodeFactory.h"
#include "NodeDefinition.h"

#include "../Nodes/Envelope/EnvelopePurpose.h"

#include <algorithm>

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
    const auto duplicate = std::find_if(edges.begin(), edges.end(), [&](const Edge& edge) {
        return edge.sourceNodeId == edgeToAdd.sourceNodeId
                && edge.sourcePortId == edgeToAdd.sourcePortId
                && edge.destNodeId == edgeToAdd.destNodeId
                && edge.destPortId == edgeToAdd.destPortId
                && edge.connectionKind == edgeToAdd.connectionKind
                && edge.attachmentType == edgeToAdd.attachmentType;
    });
    if (duplicate != edges.end()) {
        return;
    }

    edges.push_back(std::move(edgeToAdd));
    ++revision;
}

bool NodeGraph::addGuideCurve(GuideCurveResource resource) {
    if (resource.id.isEmpty() || findGuideCurve(resource.id) != nullptr) {
        return false;
    }

    guideCurves.push_back(std::move(resource));
    ++revision;
    return true;
}

bool NodeGraph::removeGuideCurve(const String& guideId) {
    const size_t previousCount = guideCurves.size();
    eraseIf(guideCurves, [&](const GuideCurveResource& resource) {
        return resource.id == guideId;
    });
    if (guideCurves.size() == previousCount) {
        return false;
    }

    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.guideId == guideId;
    });
    ++revision;
    return true;
}

const GuideCurveResource* NodeGraph::findGuideCurve(const String& guideId) const {
    for (const auto& resource : guideCurves) {
        if (resource.id == guideId) {
            return &resource;
        }
    }
    return nullptr;
}

GuideCurveResource* NodeGraph::findGuideCurveForEditing(const String& guideId) {
    for (auto& resource : guideCurves) {
        if (resource.id == guideId) {
            return &resource;
        }
    }
    return nullptr;
}

bool NodeGraph::replaceGuideCurve(GuideCurveResource resource) {
    GuideCurveResource* existing = findGuideCurveForEditing(resource.id);
    if (existing == nullptr) {
        return false;
    }

    *existing = std::move(resource);
    ++revision;
    return true;
}

bool NodeGraph::moveGuideCurve(const String& guideId, int shelfOrder) {
    const auto source = std::find_if(guideCurves.begin(), guideCurves.end(), [&](const auto& guide) {
        return guide.id == guideId;
    });
    if (source == guideCurves.end()) {
        return false;
    }

    const int sourceIndex = (int) std::distance(guideCurves.begin(), source);
    const int destinationIndex = jlimit(0, (int) guideCurves.size() - 1, shelfOrder);
    if (sourceIndex == destinationIndex) {
        return false;
    }

    if (sourceIndex < destinationIndex) {
        std::rotate(source, source + 1, guideCurves.begin() + destinationIndex + 1);
    } else {
        std::rotate(guideCurves.begin() + destinationIndex, source, source + 1);
    }
    for (int index = 0; index < (int) guideCurves.size(); ++index) {
        guideCurves[(size_t) index].shelfOrder = index;
    }
    ++revision;
    return true;
}

bool NodeGraph::assignGuideCurve(GuideCurveAssignment assignment) {
    if (findGuideCurve(assignment.guideId) == nullptr
            || findNode(assignment.targetNodeId) == nullptr
            || assignment.target.cubeIndex < 0) {
        return false;
    }

    for (auto& existing : guideAssignments) {
        if (existing.targets(assignment.targetNodeId, assignment.target)) {
            if (existing.guideId == assignment.guideId) {
                return false;
            }
            existing = std::move(assignment);
            ++revision;
            return true;
        }
    }

    guideAssignments.push_back(std::move(assignment));
    ++revision;
    return true;
}

bool NodeGraph::removeGuideAssignment(
        const String& nodeId,
        const TrimeshCubeComponentGuideTarget& target) {
    const size_t previousCount = guideAssignments.size();
    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.targets(nodeId, target);
    });
    if (guideAssignments.size() == previousCount) {
        return false;
    }

    ++revision;
    return true;
}

void NodeGraph::addSignalProbe(SignalProbe probe) {
    if (findSignalProbe(probe.id) != nullptr
            || findSignalProbeForSource(probe.sourceNodeId, probe.sourcePortId) != nullptr) {
        return;
    }

    signalProbes.push_back(std::move(probe));
    ++revision;
}

bool NodeGraph::removeSignalProbe(const String& probeId) {
    const size_t previousCount = signalProbes.size();
    eraseIf(signalProbes, [&](const SignalProbe& probe) {
        return probe.id == probeId;
    });
    if (signalProbes.size() == previousCount) {
        return false;
    }

    ++revision;
    return true;
}

const SignalProbe* NodeGraph::findSignalProbe(const String& probeId) const {
    for (const auto& probe : signalProbes) {
        if (probe.id == probeId) {
            return &probe;
        }
    }
    return nullptr;
}

SignalProbe* NodeGraph::findSignalProbeForEditing(const String& probeId) {
    for (auto& probe : signalProbes) {
        if (probe.id == probeId) {
            return &probe;
        }
    }
    return nullptr;
}

const SignalProbe* NodeGraph::findSignalProbeForSource(
        const String& sourceNodeId,
        const String& sourcePortId) const {
    for (const auto& probe : signalProbes) {
        if (probe.sourceNodeId == sourceNodeId && probe.sourcePortId == sourcePortId) {
            return &probe;
        }
    }
    return nullptr;
}

void NodeGraph::removeNode(const String& nodeId) {
    const size_t previousNodeCount = nodes.size();
    eraseIf(nodes, [&](const Node& node) {
        return node.id == nodeId;
    });

    eraseIf(edges, [&](const Edge& edge) {
        return edge.sourceNodeId == nodeId || edge.destNodeId == nodeId;
    });
    eraseIf(guideAssignments, [&](const GuideCurveAssignment& assignment) {
        return assignment.targetNodeId == nodeId;
    });
    for (auto& probe : signalProbes) {
        if (probe.sourceNodeId == nodeId) {
            probe.sourceNodeId = {};
            probe.sourcePortId = {};
        }
        if (probe.anchorDestNodeId == nodeId) {
            probe.anchorDestNodeId = {};
            probe.anchorDestPortId = {};
        }
    }
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

void NodeGraph::removeEdgesFromOutput(const String& nodeId, const String& portId) {
    const size_t previousEdgeCount = edges.size();
    eraseIf(edges, [&](const Edge& edge) {
        return edge.sourceNodeId == nodeId && edge.sourcePortId == portId;
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

bool NodeGraph::replaceNodeModel(const String& nodeId, NodeModelStatePtr model) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr) {
        return false;
    }
    if ((node->model == nullptr && model == nullptr)
            || (node->model != nullptr && model != nullptr && node->model->equals(*model))) {
        return false;
    }

    node->model = std::move(model);
    markChanged();
    return true;
}

bool NodeGraph::replaceNodeEditorState(const String& nodeId, var editorState) {
    Node* node = findNodeForEditing(nodeId);
    if (node == nullptr || JSON::toString(node->editorState, false) == JSON::toString(editorState, false)) {
        return false;
    }

    node->editorState = std::move(editorState);
    markChanged();
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

bool NodeGraph::setPerformanceKeyboardBounds(Rectangle<float> bounds) {
    if (performanceKeyboardBounds == bounds) {
        return false;
    }
    performanceKeyboardBounds = bounds;
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

String idForConnectionKind(ConnectionKind kind) {
    switch (kind) {
        case ConnectionKind::Signal:                  return "signal";
        case ConnectionKind::ConfigurationAttachment: return "configurationAttachment";
        case ConnectionKind::ProcessingAttachment:    return "processingAttachment";
    }

    return {};
}

String idForAttachmentType(AttachmentType type) {
    switch (type) {
        case AttachmentType::None:             return "none";
        case AttachmentType::ScratchEnvelope:  return "scratchEnvelope";
        case AttachmentType::ModulationTriple: return "modulationTriple";
        case AttachmentType::Unison:           return "unison";
    }

    return {};
}

std::optional<ConnectionKind> connectionKindForId(const String& id) {
    if (id == "signal") {
        return ConnectionKind::Signal;
    }
    if (id == "configurationAttachment") {
        return ConnectionKind::ConfigurationAttachment;
    }
    if (id == "processingAttachment") {
        return ConnectionKind::ProcessingAttachment;
    }
    return std::nullopt;
}

std::optional<AttachmentType> attachmentTypeForId(const String& id) {
    if (id == "none") {
        return AttachmentType::None;
    }
    if (id == "scratchEnvelope") {
        return AttachmentType::ScratchEnvelope;
    }
    if (id == "modulationTriple") {
        return AttachmentType::ModulationTriple;
    }
    if (id == "unison") {
        return AttachmentType::Unison;
    }
    return std::nullopt;
}

String parameterValueForNode(const Node& node, const String& parameterId, const String& fallback) {
    return NodeParameterMap(node).stringValue(parameterId, fallback);
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

    const float titleWidth = (float) labelForNodeKind(node.kind).length() * 8.5f;
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
