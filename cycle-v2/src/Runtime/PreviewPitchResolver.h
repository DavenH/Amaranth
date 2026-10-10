#pragma once

#include <optional>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct PreviewPitchContext {
    int midiNote { 48 };
    juce::String keyScaleAxis;
};

struct PreviewPitchBinding {
    std::optional<int> attachedMidiNote;
    juce::String keyScaleAxis;
    juce::String voiceContextNodeId;
    juce::String modulationNodeId;

    PreviewPitchContext contextForFallback(int fallbackMidiNote) const;
    PreviewPitchContext contextForPreviewNote(int previewMidiNote) const;
    void refreshFromModulationNode(const Node& node);
};

class PreviewPitchResolver {
public:
    static constexpr int defaultMidiNote = 48;

    static int forGraph(const NodeGraph& graph);
    static int forNode(
            const NodeGraph& graph,
            const String& nodeId,
            int fallbackMidiNote = defaultMidiNote);
    static PreviewPitchContext contextForNode(
            const NodeGraph& graph,
            const String& nodeId,
            int fallbackMidiNote = defaultMidiNote);
    static PreviewPitchContext contextForNodeAtPreviewNote(
            const NodeGraph& graph,
            const String& nodeId,
            int previewMidiNote);
    static PreviewPitchBinding bindingForNode(
            const NodeGraph& graph,
            const String& nodeId);
    static int forProbe(
            const NodeGraph& graph,
            const String& probeId,
            int fallbackMidiNote = defaultMidiNote);
};

}
