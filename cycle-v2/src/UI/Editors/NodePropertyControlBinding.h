#pragma once

#include <functional>

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

    const String& parameterId() const { return id; }
    std::function<void(float)> onPreviewValue;

private:
    NodeEditorCommands& commands;
    String nodeId;
    String id;
    String parameterLabel;
    bool editing {};
    bool syncing {};
};

}
