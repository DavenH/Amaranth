#pragma once

#include <App/SingletonAccessor.h>
#include "JuceHeader.h"

class CycleAutomation :
        public Timer
    ,   public SingletonAccessor {
public:
    explicit CycleAutomation(SingletonRepo* repo);

    static String stripAutomationArgs(const String& commandLine);

    void beginFromCommandLine(const String& commandLine);

private:
    struct Options {
        bool hasScript{false};
        String scriptPath;
        String reportPath;
    };

    static Options parseOptions(const String& commandLine);
    static void appendResult(Array<var>& results, const String& type, bool ok, const String& message, const var& data = {});

    void timerCallback() override;
    void runScript();
    void runCommand(const var& command, Array<var>& results);

    bool runTourAction(const var& command, String& message);
    bool captureScreenshot(const var& command, String& message, var& data);
    bool exportState(const var& command, String& message);
    bool exportPreset(const var& command, String& message);
    bool savePreset(const var& command, String& message, var& data);
    bool openPreset(const var& command, String& message, var& data);
    bool openFactoryPreset(const var& command, String& message, var& data);
    bool inspectTargets(const var& command, String& message, var& data);
    bool inspectTree(const var& command, String& message, var& data);
    bool setControl(const var& command, String& message, var& data);
    bool pointer(const var& command, String& message, var& data);
    bool assertTarget(const var& command, String& message, var& data);
    bool assertState(const var& command, String& message, var& data);
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
    bool hasRun{false};
};
