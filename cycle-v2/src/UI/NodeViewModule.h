#pragma once

#include "../Graph/NodeDefinition.h"
#include "NodeEditorHost.h"

#include <memory>
#include <optional>
#include <vector>

namespace CycleV2 {

struct NodeViewCapabilities {
    bool previewable {};
    bool hostedEditor {};
    bool expandedEditorBlocksCanvas { true };
    bool operationLayoutControl {};
    bool outputSideControl {};
    std::optional<juce::Point<float>> expandedEditorSize;
    std::optional<juce::Point<float>> expandedEditorScale;
};

class NodeViewModule {
public:
    virtual ~NodeViewModule() = default;
    virtual const NodeViewCapabilities& capabilities() const = 0;
    virtual const NodeEditorFactory* editorFactory() const = 0;
    virtual juce::Rectangle<float> expandedEditorBounds(
            juce::Rectangle<float> componentBounds,
            float minimumMargin) const = 0;
    virtual std::optional<juce::Point<float>> attachmentWorldCentre(
            const Node& node,
            const juce::String& semanticPortId) const = 0;
};

class NodeViewModuleRegistry {
public:
    static const NodeViewModuleRegistry& instance();
    const NodeViewModule& moduleFor(NodeKind kind) const;

private:
    NodeViewModuleRegistry();
    class RegisteredModule;
    std::vector<std::unique_ptr<RegisteredModule>> modules;
    std::unique_ptr<RegisteredModule> genericModule;
};

}
