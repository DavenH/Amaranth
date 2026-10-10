#include "UI/Editors/NodePropertyControlBinding.h"

#include "Graph/NodeDefinition.h"

namespace CycleV2 {

NodePropertySliderRow::NodePropertySliderRow(
        Component& owner,
        NodeEditorCommands& commandsToUse,
        String parameterId,
        String labelText) :
        PropertySliderRow(owner, labelText)
    ,   owner           (owner)
    ,   commands        (commandsToUse)
    ,   id              (std::move(parameterId))
    ,   parameterLabel  (std::move(labelText)) {
    slider.onDragStart = [this] {
        if (editing) {
            return;
        }
        editing = commands.beginNodeParameterEdit(
                nodeId,
                id,
                parameterLabel,
                (float) slider.getValue());
    };
    slider.onValueChange = [this] {
        if (syncing) {
            return;
        }
        const float value = (float) slider.getValue();
        previewValue(value);
        if (editing) {
            commands.updateNodeParameterEditValue(value);
        } else {
            commands.setNodeParameterValue(nodeId, id, parameterLabel, value);
        }
    };
    slider.onDragEnd = [this] {
        if (!editing) {
            return;
        }
        editing = false;
        commands.endNodeParameterEdit();
    };
}

void NodePropertySliderRow::mirrorPreviewInto(Node& node, NodeKind kind) {
    previewNode = &node;
    previewKind = kind;
}

void NodePropertySliderRow::previewValue(float value) {
    if (previewNode == nullptr || !previewKind.has_value()) {
        return;
    }
    const auto* definition = NodeDefinitionRegistry::instance().findParameter(
            *previewKind, id);
    const String normalized = definition != nullptr
            ? definition->normalized(String(value, 6))
            : String(value, 6);
    for (auto& parameter : previewNode->parameters) {
        if (parameter.id == id) {
            parameter.value = normalized;
            break;
        }
    }
    owner.repaint();
}

void NodePropertySliderRow::bind(const String& nextNodeId, double nextValue) {
    const ScopedValueSetter<bool> guard(syncing, true);
    nodeId = nextNodeId;
    slider.setValue(nextValue, dontSendNotification);
    refreshValueText();
}

}
