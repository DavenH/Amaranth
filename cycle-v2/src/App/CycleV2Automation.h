#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using namespace juce;

class NodeWorkspace;
class CycleV2AutomationAssertions;
class CycleV2AutomationInput;
class CycleV2AutomationSessionTransport;
class CycleV2AutomationWorkspaceCommands;

class CycleV2Automation {
public:
    struct Options {
        File scriptFile;
        File reportFile;
        String sessionPath;
        bool hasSession { false };
    };

    static Options parseCommandLine(const String& commandLine);
    static bool hasAutomation(const Options& options);

    CycleV2Automation(NodeWorkspace& workspace, Component& window, Options options);
    ~CycleV2Automation();

    void runScriptAsync();

private:
    NodeWorkspace& workspace;
    Component& window;
    Options options;
    std::unique_ptr<CycleV2AutomationSessionTransport> sessionTransport;
    std::unique_ptr<CycleV2AutomationAssertions> assertions;
    std::unique_ptr<CycleV2AutomationInput> input;
    std::unique_ptr<CycleV2AutomationWorkspaceCommands> workspaceCommands;

    var runCommand(const var& commandValue);
    void startSessionServer();
    File resolveCommandPath(const String& path) const;
    var snapshotState() const;
    var inspectTargets(const var& commandValue) const;
    var exportGraph(const var& commandValue) const;
    var openGraph(const var& commandValue);
    var saveGraph(const var& commandValue);
    var listMenuItems() const;
    var invokeMenuItem(const var& commandValue);
    var listPaletteItems() const;
    var invokePaletteItem(const var& commandValue);
    var captureAudio(const var& commandValue);
    var captureLiveAudio(const var& commandValue);
    var sendMidi(const var& commandValue);
    var inspectPointerTargets() const;
    var inspectPointerCursor() const;
    var inspectOpenGLDiagnostics() const;
    var inspectCanvasPerformance() const;
    var resetCanvasPerformance();
    var inspectAudioPerformance() const;
    var resetAudioPerformance();
    var requestCanvasOpenGLFrame();
    var screenshot(const var& commandValue) const;
    var waitForIdle(const var& commandValue) const;

    Component* componentForArea(const String& area) const;
    var componentInfo(const String& area, Component& component) const;
    void writeReport(const var& report) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CycleV2Automation)
};

}
