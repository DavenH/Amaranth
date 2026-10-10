#pragma once

#include <optional>

#include "UI/Editors/PropertyControls.h"
#include "UI/NodeEditorHost.h"

namespace CycleV2 {

class NodePropertySliderRow final : public PropertySliderRow {
public:
    NodePropertySliderRow(
            Component& owner,
            NodeEditorCommands& commands,
            String parameterId,
            String label);

    void bind(const String& nodeId, double value);
    void mirrorPreviewInto(Node& node, NodeKind kind);
    void previewValue(float value);

    const String& parameterId() const { return id; }

private:
    Component& owner;
    NodeEditorCommands& commands;
    Node* previewNode {};
    std::optional<NodeKind> previewKind;
    String nodeId;
    String id;
    String parameterLabel;
    bool editing {};
    bool syncing {};
};

}
