#include "GraphSerializer.h"

#include <memory>

namespace CycleV2 {

namespace {

Identifier graphType("cycleV2Graph");
Identifier nodeType("node");
Identifier inputType("input");
Identifier outputType("output");
Identifier edgeType("edge");

String idForNodeKind(NodeKind kind) {
    switch (kind) {
        case NodeKind::VoiceContext:                 return "voiceContext";
        case NodeKind::TrilinearWaveSurface:         return "trilinearWaveSurface";
        case NodeKind::Fft:                          return "fft";
        case NodeKind::SpectralMagnitudeProcessor:   return "spectralMagnitudeProcessor";
        case NodeKind::SpectralPhaseProcessor:       return "spectralPhaseProcessor";
        case NodeKind::Ifft:                         return "ifft";
        case NodeKind::Envelope:                     return "envelope";
        case NodeKind::Multiply:                     return "multiply";
        case NodeKind::Output:                       return "output";
        default:                                     return "genericProcessor";
    }
}

NodeKind nodeKindForId(const String& id) {
    if (id == "voiceContext") {
        return NodeKind::VoiceContext;
    }
    if (id == "trilinearWaveSurface") {
        return NodeKind::TrilinearWaveSurface;
    }
    if (id == "fft") {
        return NodeKind::Fft;
    }
    if (id == "spectralMagnitudeProcessor") {
        return NodeKind::SpectralMagnitudeProcessor;
    }
    if (id == "spectralPhaseProcessor") {
        return NodeKind::SpectralPhaseProcessor;
    }
    if (id == "ifft") {
        return NodeKind::Ifft;
    }
    if (id == "envelope") {
        return NodeKind::Envelope;
    }
    if (id == "multiply") {
        return NodeKind::Multiply;
    }
    if (id == "output") {
        return NodeKind::Output;
    }

    return NodeKind::GenericProcessor;
}

String idForDomain(PortDomain domain) {
    switch (domain) {
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

ValueTree portToTree(const Port& port, Identifier type) {
    ValueTree tree(type);
    tree.setProperty("id", port.id, nullptr);
    tree.setProperty("label", port.label, nullptr);
    tree.setProperty("domain", idForDomain(port.domain), nullptr);
    tree.setProperty("channelLayout", idForChannelLayout(port.channelLayout), nullptr);
    tree.setProperty("purpose", idForPortPurpose(port.purpose), nullptr);
    return tree;
}

Port portFromTree(const ValueTree& tree, bool input) {
    return {
            tree["id"].toString(),
            tree["label"].toString(),
            domainForId(tree["domain"].toString()),
            channelLayoutForId(tree["channelLayout"].toString()),
            portPurposeForId(tree["purpose"].toString()),
            input
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
            node.id = child["id"].toString();
            node.kind = nodeKindForId(child["kind"].toString());
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
                }
            }

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
