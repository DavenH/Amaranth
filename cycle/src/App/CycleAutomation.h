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
    bool inspectTargets(const var& command, String& message, var& data);
    bool setControl(const var& command, String& message, var& data);
    bool assertTarget(const var& command, String& message, var& data);
    bool assertState(const var& command, String& message, var& data);
    bool waitForIdle(const var& command, String& message, var& data);
    var  snapshotState();

    Component* resolveComponent(const var& command);
    var componentState(Component* component, const String& area, const String& target) const;

    Options options;
    bool hasRun{false};
};
