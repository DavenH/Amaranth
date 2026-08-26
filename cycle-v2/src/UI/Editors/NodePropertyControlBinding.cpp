#include "UI/Editors/NodePropertyControlBinding.h"

namespace CycleV2 {

NodePropertySliderRow::NodePropertySliderRow(
        Component& owner,
        NodeEditorCommands& commandsToUse,
        String parameterId,
        String labelText) :
        PropertySliderRow(owner, labelText)
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
        if (onPreviewValue != nullptr) {
            onPreviewValue(value);
        }
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

void NodePropertySliderRow::bind(const String& nextNodeId, double nextValue) {
    const ScopedValueSetter<bool> guard(syncing, true);
    nodeId = nextNodeId;
    slider.setValue(nextValue, dontSendNotification);
    refreshValueText();
}

}
