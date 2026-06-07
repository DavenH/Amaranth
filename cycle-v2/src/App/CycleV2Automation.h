#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using namespace juce;

class NodeWorkspace;

class CycleV2Automation {
public:
    struct Options {
        File scriptFile;
        File reportFile;
    };

    static Options parseCommandLine(const String& commandLine);
    static bool hasAutomation(const Options& options);

    CycleV2Automation(NodeWorkspace& workspace, Component& window, Options options);

    void runScriptAsync();

private:
    NodeWorkspace& workspace;
    Component& window;
    Options options;

    var runCommand(const var& commandValue);
    var snapshotState() const;
    var inspectTargets(const var& commandValue) const;
    var exportGraph(const var& commandValue) const;
    var openGraph(const var& commandValue);
    var saveGraph(const var& commandValue);
    var openNodeEditor(const var& commandValue);
    var addNode(const var& commandValue);
    var moveNode(const var& commandValue);
    var connectPorts(const var& commandValue);
    var deleteNode(const var& commandValue);
    var deleteEdge(const var& commandValue);
    var setNodeParameter(const var& commandValue);
    var inspectNodeControls(const var& commandValue) const;
    var setMorphSlider(const var& commandValue);
    var setPrimaryAxis(const var& commandValue);
    var toggleLink(const var& commandValue);
    var selectVertex(const var& commandValue);
    var setVertexParameter(const var& commandValue);
    var pointer(const var& commandValue);
    var screenshot(const var& commandValue) const;
    var assertState(const var& commandValue) const;
    var assertNodeParameter(const var& commandValue) const;
    var listAssertionPaths() const;
    var waitForIdle(const var& commandValue) const;

    Component* componentForArea(const String& area) const;
    var componentInfo(const String& area, Component& component) const;
    void writeReport(const var& report) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CycleV2Automation)
};

}
