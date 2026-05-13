#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class CycleAutomation :
        public Timer
    ,   public SingletonAccessor {
public:
    explicit CycleAutomation(SingletonRepo* repo);
    ~CycleAutomation() override;

    static String stripAutomationArgs(const String& commandLine);

    void beginFromCommandLine(const String& commandLine);

private:
    struct Options {
        bool hasScript{false};
        bool hasSession{false};
        String scriptPath;
        String reportPath;
        String sessionPath;
    };

    class SessionServer;

    static Options parseOptions(const String& commandLine);
    static void appendResult(Array<var>& results, const String& type, bool ok, const String& message, const var& data = {});

    void timerCallback() override;
    void runScript();
    void runCommand(const var& command, Array<var>& results);
    var runCommandResult(const var& command);
    var handleSessionRequest(const var& request);
    void startSessionServer();

    bool runTourAction(const var& command, String& message);
    bool captureScreenshot(const var& command, String& message, var& data);
    bool captureAudio(const var& command, String& message, var& data);
    bool exportState(const var& command, String& message);
    bool exportPreset(const var& command, String& message);
    bool savePreset(const var& command, String& message, var& data);
    bool openPreset(const var& command, String& message, var& data);
    bool openFactoryPreset(const var& command, String& message, var& data);
    bool listMenus(const var& command, String& message, var& data);
    bool invokeMenuItem(const var& command, String& message, var& data);
    bool listModMatrixMenu(const var& command, String& message, var& data);
    bool invokeModMatrixMenu(const var& command, String& message, var& data);
    bool listModMatrixDimensionMenu(const var& command, String& message, var& data);
    bool invokeModMatrixDimensionMenu(const var& command, String& message, var& data);
    bool listEnvelopeConfigMenu(const var& command, String& message, var& data);
    bool invokeEnvelopeConfigMenu(const var& command, String& message, var& data);
    bool listSelectorMenu(const var& command, String& message, var& data);
    bool invokeSelectorMenu(const var& command, String& message, var& data);
    bool inspectTargets(const var& command, String& message, var& data);
    bool inspectTree(const var& command, String& message, var& data);
    bool setControl(const var& command, String& message, var& data);
    bool pointer(const var& command, String& message, var& data);
    bool assertTarget(const var& command, String& message, var& data);
    bool assertState(const var& command, String& message, var& data);
    bool listAssertionPaths(const var& command, String& message, var& data);
    bool listMeshTargets(const var& command, String& message, var& data);
    bool exportMeshState(const var& command, String& message, var& data);
    bool mutateMeshVertex(const var& command, String& message, var& data);
    bool meshSelectionGesture(const var& command, String& message, var& data);
    bool waitForIdle(const var& command, String& message, var& data);
    var  snapshotState();

    Component* resolveComponent(const var& command);
    var componentState(Component* component, const String& area, const String& target) const;
    var registeredTargetsForRoot(Component* root, const String& requestedArea) const;
    var componentTreeState(Component* component,
                           const String& path,
                           int depth,
                           int maxDepth,
                           int maxNodes,
                           int& nodeCount,
                           bool includeInvisible) const;
    void annotateTourTarget(DynamicObject& json, Component* component) const;

    Options options;
    std::unique_ptr<SessionServer> sessionServer;
    bool hasRun{false};
};
