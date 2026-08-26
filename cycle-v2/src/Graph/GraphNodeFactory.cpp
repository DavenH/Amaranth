#include "Graph/GraphNodeFactory.h"

#include "Graph/NodeDefinition.h"

#include "Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

Node GraphNodeFactory::createNode(NodeKind kind, const String& id, Point<float> position) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    if (definition == nullptr) {
        definition = NodeDefinitionRegistry::instance().find(NodeKind::GenericProcessor);
    }

    jassert(definition != nullptr);
    Node node;
    node.id = id;
    node.kind = definition->kind;
    node.subtitle = definition->subtitle;
    node.bounds = { position.x, position.y, 240.f, 170.f };
    node.inputs = definition->inputs;
    node.outputs = definition->outputs;
    node.parameters.reserve(definition->parameters.size());
    for (const auto& parameter : definition->parameters) {
        node.parameters.push_back({ parameter.id, parameter.label, parameter.defaultValue });
    }
    if (definition->modelCodec != nullptr) {
        node.model = definition->modelCodec->createDefault();
    }
    NodeDefinitionRegistry::instance().normalize(node);

    const auto naturalSize = naturalSizeForNode(node);
    node.bounds.setSize(naturalSize.width, naturalSize.height);
    return node;
}

Port GraphNodeFactory::input(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout,
        PortPurpose purpose,
        PortSide side) const {
    return {
            std::move(id), std::move(label), domain, layout, purpose, true, side,
            purpose == PortPurpose::ScratchAttachment
                    ? ConnectionKind::ProcessingAttachment
                    : ConnectionKind::Signal,
            purpose == PortPurpose::ScratchAttachment
                    ? AttachmentType::ScratchEnvelope
                    : AttachmentType::None
    };
}

Port GraphNodeFactory::output(
        String id,
        String label,
        PortDomain domain,
        ChannelLayout layout,
        PortSide side) const {
    return { std::move(id), std::move(label), domain, layout, PortPurpose::Signal, false, side };
}

}
