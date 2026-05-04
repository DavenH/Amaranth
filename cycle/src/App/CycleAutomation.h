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
    static void appendResult(Array<var>& results, const String& type, bool ok, const String& message);

    void timerCallback() override;
    void runScript();
    void runCommand(const var& command, Array<var>& results);

    bool runTourAction(const var& command, String& message);
    bool captureScreenshot(const var& command, String& message);
    bool exportState(const var& command, String& message);
    var  snapshotState();

    Component* resolveComponent(const var& command);

    Options options;
    bool hasRun{false};
};
