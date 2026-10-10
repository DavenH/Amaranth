#pragma once

#include <unordered_map>
#include <vector>

#include "Runtime/PreviewPitchResolver.h"

namespace CycleV2 {

class PreviewPitchContextIndex {
public:
    void rebuild(const NodeGraph& graph);
    void applyParameterChanges(
            const NodeGraph& graph,
            const std::vector<juce::String>& changedNodeIds,
            bool topologyChanged);
    PreviewPitchContext contextForNodeAtPreviewNote(
            const juce::String& nodeId,
            int previewMidiNote) const;

    size_t graphResolutionCount() const { return graphResolutions; }
    size_t parameterRefreshCount() const { return parameterRefreshes; }

private:
    struct StringHash {
        size_t operator()(const juce::String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    using BindingMap = std::unordered_map<juce::String, PreviewPitchBinding, StringHash>;
    using DependentMap = std::unordered_map<
            juce::String,
            std::vector<juce::String>,
            StringHash>;

    BindingMap bindings;
    DependentMap dependentsByModulationNode;
    bool initialized {};
    size_t graphResolutions {};
    size_t parameterRefreshes {};
};

}
