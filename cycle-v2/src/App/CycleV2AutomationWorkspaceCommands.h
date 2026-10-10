#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class NodeWorkspace;

class CycleV2AutomationWorkspaceCommands {
public:
    using SnapshotProvider = std::function<juce::var()>;
    using PathResolver = std::function<juce::File(const juce::String&)>;

    CycleV2AutomationWorkspaceCommands(
            NodeWorkspace& workspace,
            SnapshotProvider snapshotProvider,
            PathResolver pathResolver);

    juce::var openNodeEditor(const juce::var& command);
    juce::var addNode(const juce::var& command);
    juce::var moveNode(const juce::var& command);
    juce::var connectPorts(const juce::var& command);
    juce::var deleteNode(const juce::var& command);
    juce::var deleteEdge(const juce::var& command);
    juce::var deleteGuideCurve(const juce::var& command);
    juce::var loadGuideHeatmap(const juce::var& command);
    juce::var clearGuideHeatmap(const juce::var& command);
    juce::var undo();
    juce::var setGuideParameter(const juce::var& command);
    juce::var setNodeParameter(const juce::var& command);
    juce::var inspectNodeControls(const juce::var& command) const;
    juce::var setMorphSlider(const juce::var& command);
    juce::var setPrimaryAxis(const juce::var& command);
    juce::var toggleLink(const juce::var& command);
    juce::var selectVertex(const juce::var& command);
    juce::var setVertexParameter(const juce::var& command);

private:
    NodeWorkspace& workspace;
    SnapshotProvider snapshot;
    PathResolver resolvePath;
};

}
