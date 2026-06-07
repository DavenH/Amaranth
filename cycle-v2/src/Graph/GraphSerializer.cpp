#include "GraphSerializer.h"

#include <memory>

namespace CycleV2 {

namespace {

Identifier graphType("cycleV2Graph");
Identifier nodeType("node");
Identifier inputType("input");
Identifier outputType("output");
Identifier edgeType("edge");
Identifier parameterType("parameter");

String idForNodeKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::VoiceContext:                 return "voiceContext";
        case NodeKind::WaveSource:                   return "waveSource";
        case NodeKind::ImageSource:                  return "imageSource";
        case NodeKind::TrilinearMesh:                return "trilinearMesh";
        case NodeKind::Fft:                          return "fft";
        case NodeKind::Ifft:                         return "ifft";
        case NodeKind::Envelope:                     return "envelope";
        case NodeKind::Add:                          return "add";
        case NodeKind::Multiply:                     return "multiply";
        case NodeKind::GuideCurve:                   return "guideCurve";
        case NodeKind::ImpulseResponse:              return "impulseResponse";
        case NodeKind::Waveshaper:                   return "waveshaper";
        case NodeKind::Reverb:                       return "reverb";
        case NodeKind::Delay:                        return "delay";
        case NodeKind::StereoSplit:                  return "stereoSplit";
        case NodeKind::StereoJoin:                   return "stereoJoin";
        case NodeKind::Output:                       return "output";
        default:                                     return "genericProcessor";
    }
}

NodeKind nodeKindForId(const String& id) {
    if (id == "voiceContext") {
        return NodeKind::VoiceContext;
    }
    if (id == "waveSource") {
        return NodeKind::WaveSource;
    }
    if (id == "imageSource") {
        return NodeKind::ImageSource;
    }
    if (id == "trilinearMesh") {
        return NodeKind::TrilinearMesh;
    }
    if (id == "fft") {
        return NodeKind::Fft;
    }
    if (id == "ifft") {
        return NodeKind::Ifft;
    }
    if (id == "envelope") {
        return NodeKind::Envelope;
    }
    if (id == "add") {
        return NodeKind::Add;
    }
    if (id == "multiply") {
        return NodeKind::Multiply;
    }
    if (id == "guideCurve") {
        return NodeKind::GuideCurve;
    }
    if (id == "impulseResponse") {
        return NodeKind::ImpulseResponse;
    }
    if (id == "waveshaper") {
        return NodeKind::Waveshaper;
    }
    if (id == "reverb") {
        return NodeKind::Reverb;
    }
    if (id == "delay") {
        return NodeKind::Delay;
    }
    if (id == "stereoSplit") {
        return NodeKind::StereoSplit;
    }
    if (id == "stereoJoin") {
        return NodeKind::StereoJoin;
    }
    if (id == "output") {
        return NodeKind::Output;
    }

    return NodeKind::GenericProcessor;
}

void normalizeNodePresentation(Node& node) {
    auto ensureParameter = [&](const String& id, const String& label, const String& value) {
        for (const auto& parameter : node.parameters) {
            if (parameter.id == id) {
                return;
            }
        }

        node.parameters.push_back({ id, label, value });
    };

    if (node.kind == NodeKind::VoiceContext) {
        ensureParameter("domain", "Start Domain", "waveform");
        ensureParameter("voices", "Voices", "1");
        ensureParameter("octave", "Octave", "0");
        ensureParameter("pitch", "Pitch", "0");
        ensureParameter("portamento", "Portamento", "0");
        ensureParameter("oversampling", "Oversampling", "1x");
    }

    if (node.kind == NodeKind::Fft) {
        ensureParameter("cycleFrames", "Cycle Frames", "2048");
        ensureParameter("mode", "Mode", "cycle");
        node.title = String::fromUTF8("Time → Freq");
        node.subtitle = parameterValueForNode(node, "mode", "cycle") == "fixedWindow" ? "fixed window" : "cycle chunks";
    }

    if (node.kind == NodeKind::Ifft) {
        ensureParameter("cycleFrames", "Cycle Frames", "2048");
        ensureParameter("mode", "Mode", "cyclic");
        node.title = String::fromUTF8("Freq → Time");
        node.subtitle = parameterValueForNode(node, "mode", "cyclic") == "acyclicCarry"
                ? "carry overlap"
                : "cyclic overlap";
    }

    if (node.kind == NodeKind::VoiceContext
            || node.kind == NodeKind::Fft
            || node.kind == NodeKind::Ifft
            || node.kind == NodeKind::Add
            || node.kind == NodeKind::Multiply
            || node.kind == NodeKind::Output) {
        const auto naturalSize = naturalSizeForNode(node);
        node.bounds.setSize(naturalSize.width, naturalSize.height);
    }
}

String idForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::DomainContext:              return "domain";
        case PortDomain::TimeSignal:                 return "time";
        case PortDomain::SpectralMagnitudeSignal:    return "spectralMagnitude";
        case PortDomain::SpectralPhaseSignal:        return "spectralPhase";
        case PortDomain::MeshField:                  return "mesh";
        case PortDomain::EnvelopeSignal:             return "envelope";
        case PortDomain::PitchSignal:                return "pitch";
        case PortDomain::VoiceControlSignal:         return "voice";
        case PortDomain::ControlSignal:              return "control";
        default:                                     return "control";
    }
}

PortDomain domainForId(const String& id) {
    if (id == "domain") {
        return PortDomain::DomainContext;
    }
    if (id == "time") {
        return PortDomain::TimeSignal;
    }
    if (id == "spectralMagnitude") {
        return PortDomain::SpectralMagnitudeSignal;
    }
    if (id == "spectralPhase") {
        return PortDomain::SpectralPhaseSignal;
    }
    if (id == "mesh") {
        return PortDomain::MeshField;
    }
    if (id == "envelope") {
        return PortDomain::EnvelopeSignal;
    }
    if (id == "pitch") {
        return PortDomain::PitchSignal;
    }
    if (id == "voice") {
        return PortDomain::VoiceControlSignal;
    }

    return PortDomain::ControlSignal;
}

String idForChannelLayout(ChannelLayout layout) {
    switch (layout) {
        case ChannelLayout::LinkedStereo: return "linkedStereo";
        case ChannelLayout::Left:         return "left";
        case ChannelLayout::Right:        return "right";
        case ChannelLayout::StereoPair:   return "stereoPair";
        default:                          return "mono";
    }
}

ChannelLayout channelLayoutForId(const String& id) {
    if (id == "linkedStereo") {
        return ChannelLayout::LinkedStereo;
    }
    if (id == "left") {
        return ChannelLayout::Left;
    }
    if (id == "right") {
        return ChannelLayout::Right;
    }
    if (id == "stereoPair") {
        return ChannelLayout::StereoPair;
    }

    return ChannelLayout::Mono;
}

String idForPortPurpose(PortPurpose purpose) {
    return purpose == PortPurpose::ScratchAttachment ? "scratchAttachment" : "signal";
}

PortPurpose portPurposeForId(const String& id) {
    return id == "scratchAttachment" ? PortPurpose::ScratchAttachment : PortPurpose::Signal;
}

String idForPortSide(PortSide side) {
    switch (side) {
        case PortSide::Right:     return "right";
        case PortSide::Top:       return "top";
        case PortSide::Bottom:    return "bottom";
        default:                  return "left";
    }
}

PortSide portSideForId(const String& id, bool input) {
    if (id == "right") {
        return PortSide::Right;
    }
    if (id == "top") {
        return PortSide::Top;
    }
    if (id == "bottom") {
        return PortSide::Bottom;
    }

    return input ? PortSide::Left : PortSide::Right;
}

ValueTree portToTree(const Port& port, Identifier type) {
    ValueTree tree(type);
    tree.setProperty("id", port.id, nullptr);
    tree.setProperty("label", port.label, nullptr);
    tree.setProperty("domain", idForDomain(port.domain), nullptr);
    tree.setProperty("channelLayout", idForChannelLayout(port.channelLayout), nullptr);
    tree.setProperty("purpose", idForPortPurpose(port.purpose), nullptr);
    tree.setProperty("side", idForPortSide(port.side), nullptr);
    return tree;
}

ValueTree parameterToTree(const NodeParameter& parameter) {
    ValueTree tree(parameterType);
    tree.setProperty("id", parameter.id, nullptr);
    tree.setProperty("label", parameter.label, nullptr);
    tree.setProperty("value", parameter.value, nullptr);
    return tree;
}

Port portFromTree(const ValueTree& tree, bool input) {
    const String side = tree.hasProperty("side")
            ? tree["side"].toString()
            : String();

    return {
            tree["id"].toString(),
            tree["label"].toString(),
            domainForId(tree["domain"].toString()),
            channelLayoutForId(tree["channelLayout"].toString()),
            portPurposeForId(tree["purpose"].toString()),
            input,
            portSideForId(side, input)
    };
}

NodeParameter parameterFromTree(const ValueTree& tree) {
    return {
            tree["id"].toString(),
            tree["label"].toString(),
            tree["value"].toString()
    };
}

}

ValueTree GraphSerializer::toValueTree(const NodeGraph& graph) const {
    ValueTree root(graphType);

    for (const auto& node : graph.getNodes()) {
        ValueTree nodeTree(nodeType);
        nodeTree.setProperty("id", node.id, nullptr);
        nodeTree.setProperty("kind", idForNodeKind(node.kind), nullptr);
        nodeTree.setProperty("title", node.title, nullptr);
        nodeTree.setProperty("subtitle", node.subtitle, nullptr);
        nodeTree.setProperty("x", node.bounds.getX(), nullptr);
        nodeTree.setProperty("y", node.bounds.getY(), nullptr);
        nodeTree.setProperty("w", node.bounds.getWidth(), nullptr);
        nodeTree.setProperty("h", node.bounds.getHeight(), nullptr);

        for (const auto& port : node.inputs) {
            nodeTree.addChild(portToTree(port, inputType), -1, nullptr);
        }

        for (const auto& port : node.outputs) {
            nodeTree.addChild(portToTree(port, outputType), -1, nullptr);
        }

        for (const auto& parameter : node.parameters) {
            nodeTree.addChild(parameterToTree(parameter), -1, nullptr);
        }

        root.addChild(nodeTree, -1, nullptr);
    }

    for (const auto& edge : graph.getEdges()) {
        ValueTree edgeTree(edgeType);
        edgeTree.setProperty("sourceNodeId", edge.sourceNodeId, nullptr);
        edgeTree.setProperty("sourcePortId", edge.sourcePortId, nullptr);
        edgeTree.setProperty("destNodeId", edge.destNodeId, nullptr);
        edgeTree.setProperty("destPortId", edge.destPortId, nullptr);
        edgeTree.setProperty("domain", idForDomain(edge.domain), nullptr);
        edgeTree.setProperty("attachment", edge.attachment, nullptr);
        root.addChild(edgeTree, -1, nullptr);
    }

    return root;
}

NodeGraph GraphSerializer::fromValueTree(const ValueTree& tree) const {
    NodeGraph graph;

    for (const auto& child : tree) {
        if (child.hasType(nodeType)) {
            Node node;
            const String kindId = child["kind"].toString();
            node.id = child["id"].toString();
            node.kind = nodeKindForId(kindId);
            node.title = child["title"].toString();
            node.subtitle = child["subtitle"].toString();
            node.bounds = {
                    (float) child["x"],
                    (float) child["y"],
                    (float) child["w"],
                    (float) child["h"]
            };

            for (const auto& portTree : child) {
                if (portTree.hasType(inputType)) {
                    node.inputs.push_back(portFromTree(portTree, true));
                } else if (portTree.hasType(outputType)) {
                    node.outputs.push_back(portFromTree(portTree, false));
                } else if (portTree.hasType(parameterType)) {
                    node.parameters.push_back(parameterFromTree(portTree));
                }
            }

            normalizeNodePresentation(node);
            graph.addNode(std::move(node));
        } else if (child.hasType(edgeType)) {
            graph.addEdge({
                    child["sourceNodeId"].toString(),
                    child["sourcePortId"].toString(),
                    child["destNodeId"].toString(),
                    child["destPortId"].toString(),
                    domainForId(child["domain"].toString()),
                    (bool) child["attachment"]
            });
        }
    }

    return graph;
}

String GraphSerializer::toXmlString(const NodeGraph& graph) const {
    auto xml = toValueTree(graph).createXml();

    if (xml == nullptr) {
        return {};
    }

    return xml->toString();
}

NodeGraph GraphSerializer::fromXmlString(const String& xml) const {
    std::unique_ptr<XmlElement> root = parseXML(xml);

    if (root == nullptr) {
        return {};
    }

    return fromValueTree(ValueTree::fromXml(*root));
}

}
