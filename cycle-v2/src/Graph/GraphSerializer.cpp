#include "GraphSerializer.h"

#include "GraphNodeFactory.h"
#include "GraphValidator.h"
#include "NodeDefinition.h"

#include <memory>

namespace CycleV2 {

namespace {

Identifier graphType("cycleV2Graph");
Identifier nodeType("node");
Identifier inputType("input");
Identifier outputType("output");
Identifier edgeType("edge");
Identifier probeType("probe");
Identifier parameterType("parameter");

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

ValueTree migrateToCurrentFormat(const ValueTree& source, std::vector<GraphLoadIssue>& issues) {
    ValueTree migrated = source.createCopy();
    int version = migrated.hasProperty("formatVersion") ? (int) migrated["formatVersion"] : 1;
    if (version < 1 || version > GraphSerializer::currentFormatVersion) {
        issues.push_back({
                GraphLoadCode::UnsupportedVersion,
                "Unsupported Cycle 2 graph format version " + String(version)
        });
        return {};
    }

    while (version < GraphSerializer::currentFormatVersion) {
        if (version == 1) {
            for (auto child : migrated) {
                if (child.hasType(nodeType) && !child.hasProperty("definitionVersion")) {
                    child.setProperty("definitionVersion", 1, nullptr);
                }
            }
            version = 2;
            migrated.setProperty("formatVersion", version, nullptr);
            continue;
        }
        issues.push_back({ GraphLoadCode::UnsupportedVersion, "No migration is available" });
        return {};
    }
    return migrated;
}

}

ValueTree GraphSerializer::toValueTree(const NodeGraph& graph) const {
    ValueTree root(graphType);
    root.setProperty("formatVersion", currentFormatVersion, nullptr);

    for (const auto& node : graph.getNodes()) {
        ValueTree nodeTree(nodeType);
        const auto& registry = NodeDefinitionRegistry::instance();
        const auto* definition = registry.find(node.kind);
        nodeTree.setProperty("id", node.id, nullptr);
        nodeTree.setProperty("kind", registry.typeIdFor(node.kind), nullptr);
        nodeTree.setProperty("definitionVersion", definition != nullptr ? definition->version : 1, nullptr);
        nodeTree.setProperty("title", node.title, nullptr);
        nodeTree.setProperty("subtitle", node.subtitle, nullptr);
        nodeTree.setProperty("x", node.bounds.getX(), nullptr);
        nodeTree.setProperty("y", node.bounds.getY(), nullptr);

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

    for (const auto& probe : graph.getSignalProbes()) {
        ValueTree probeTree(probeType);
        probeTree.setProperty("id", probe.id, nullptr);
        probeTree.setProperty("sourceNodeId", probe.sourceNodeId, nullptr);
        probeTree.setProperty("sourcePortId", probe.sourcePortId, nullptr);
        probeTree.setProperty("anchorDestNodeId", probe.anchorDestNodeId, nullptr);
        probeTree.setProperty("anchorDestPortId", probe.anchorDestPortId, nullptr);
        probeTree.setProperty("label", probe.label, nullptr);
        probeTree.setProperty("tapPosition", probe.tapPosition, nullptr);
        probeTree.setProperty("railOrder", probe.railOrder, nullptr);
        root.addChild(probeTree, -1, nullptr);
    }

    return root;
}

NodeGraph GraphSerializer::fromValueTree(const ValueTree& tree) const {
    return loadValueTree(tree).graph;
}

GraphLoadResult GraphSerializer::loadValueTree(const ValueTree& tree) const {
    GraphLoadResult result;
    if (!tree.isValid() || !tree.hasType(graphType)) {
        result.issues.push_back({ GraphLoadCode::InvalidXml, "Root element is not a Cycle 2 graph" });
        return result;
    }

    const ValueTree migrated = migrateToCurrentFormat(tree, result.issues);
    if (!result.issues.empty()) {
        return result;
    }

    const auto& registry = NodeDefinitionRegistry::instance();

    for (const auto& child : migrated) {
        if (child.hasType(nodeType)) {
            Node node;
            const String kindId = child["kind"].toString();
            const auto* definition = registry.find(kindId);
            if (definition == nullptr) {
                result.issues.push_back({ GraphLoadCode::UnknownNodeType, "Unknown node type '" + kindId + "'" });
                continue;
            }
            node.id = child["id"].toString();
            node.kind = definition->kind;
            node.title = child["title"].toString();
            node.subtitle = child["subtitle"].toString();
            node.bounds.setPosition((float) child["x"], (float) child["y"]);

            for (const auto& portTree : child) {
                if (portTree.hasType(inputType)) {
                    node.inputs.push_back(portFromTree(portTree, true));
                } else if (portTree.hasType(outputType)) {
                    node.outputs.push_back(portFromTree(portTree, false));
                } else if (portTree.hasType(parameterType)) {
                    node.parameters.push_back(parameterFromTree(portTree));
                }
            }

            registry.normalize(node);
            const NodeNaturalSize naturalSize = naturalSizeForNode(node);
            node.bounds.setSize(naturalSize.width, naturalSize.height);
            result.graph.addNode(std::move(node));
        } else if (child.hasType(edgeType)) {
            result.graph.addEdge({
                    child["sourceNodeId"].toString(),
                    child["sourcePortId"].toString(),
                    child["destNodeId"].toString(),
                    child["destPortId"].toString(),
                    domainForId(child["domain"].toString()),
                    (bool) child["attachment"]
            });
        } else if (child.hasType(probeType)) {
            result.graph.addSignalProbe({
                    child["id"].toString(),
                    child["sourceNodeId"].toString(),
                    child["sourcePortId"].toString(),
                    child["anchorDestNodeId"].toString(),
                    child["anchorDestPortId"].toString(),
                    child["label"].toString(),
                    jlimit(0.f, 1.f, (float) child["tapPosition"]),
                    (int) child["railOrder"]
            });
        }
    }

    if (result.issues.empty()) {
        for (const auto& issue : GraphValidator().validate(result.graph)) {
            result.issues.push_back({ GraphLoadCode::InvalidGraph, issue.message });
        }
    }
    return result;
}

String GraphSerializer::toXmlString(const NodeGraph& graph) const {
    auto xml = toValueTree(graph).createXml();

    if (xml == nullptr) {
        return {};
    }

    return xml->toString();
}

NodeGraph GraphSerializer::fromXmlString(const String& xml) const {
    return loadXmlString(xml).graph;
}

GraphLoadResult GraphSerializer::loadXmlString(const String& xml) const {
    std::unique_ptr<XmlElement> root = parseXML(xml);

    if (root == nullptr) {
        GraphLoadResult result;
        result.issues.push_back({ GraphLoadCode::InvalidXml, "Could not parse Cycle 2 graph XML" });
        return result;
    }

    return loadValueTree(ValueTree::fromXml(*root));
}

}
