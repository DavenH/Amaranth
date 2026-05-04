#include "CycleAutomation.h"

#include <App/Doc/Document.h>
#include <App/Doc/PresetJson.h>
#include <Definitions.h>
#include <UI/Panels/Panel.h>

#include "CycleTour.h"

#include "../UI/Panels/MainPanel.h"

namespace {
    String getString(const var& object, const Identifier& name, const String& fallback = {}) {
        return PresetJson::stringProperty(object, name, fallback);
    }

    bool getBool(const var& object, const Identifier& name, bool fallback = false) {
        return PresetJson::boolProperty(object, name, fallback);
    }

    var makeResult(const String& type, bool ok, const String& message) {
        auto result = PresetJson::object();
        result->setProperty("type", type);
        result->setProperty("ok", ok);
        result->setProperty("message", message);
        return PresetJson::toVar(result);
    }
}

CycleAutomation::CycleAutomation(SingletonRepo* repo) :
        SingletonAccessor(repo, "CycleAutomation") {
}

CycleAutomation::Options CycleAutomation::parseOptions(const String& commandLine) {
    Options parsed;
    StringArray tokens;
    tokens.addTokens(commandLine, true);
    tokens.trim();
    tokens.removeEmptyStrings();

    for (int i = 0; i < tokens.size(); ++i) {
        const String& token = tokens[i];

        if (token == "--agent-script" && i + 1 < tokens.size()) {
            parsed.hasScript = true;
            parsed.scriptPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-script=")) {
            parsed.hasScript = true;
            parsed.scriptPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        } else if (token == "--agent-report" && i + 1 < tokens.size()) {
            parsed.reportPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-report=")) {
            parsed.reportPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        }
    }

    return parsed;
}

String CycleAutomation::stripAutomationArgs(const String& commandLine) {
    StringArray tokens;
    tokens.addTokens(commandLine, true);
    tokens.trim();
    tokens.removeEmptyStrings();

    StringArray kept;

    for (int i = 0; i < tokens.size(); ++i) {
        const String& token = tokens[i];

        if ((token == "--agent-script" || token == "--agent-report") && i + 1 < tokens.size()) {
            ++i;
            continue;
        }

        if (token.startsWith("--agent-script=") || token.startsWith("--agent-report=")) {
            continue;
        }

        kept.add(token);
    }

    return kept.joinIntoString(" ");
}

void CycleAutomation::beginFromCommandLine(const String& commandLine) {
    options = parseOptions(commandLine);

    if (options.hasScript && !hasRun) {
        startTimer(500);
    }
}

void CycleAutomation::appendResult(Array<var>& results, const String& type, bool ok, const String& message) {
    results.add(makeResult(type, ok, message));
}

void CycleAutomation::timerCallback() {
    stopTimer();
    runScript();
}

void CycleAutomation::runScript() {
    hasRun = true;

    Array<var> results;
    File scriptFile(options.scriptPath);
    bool shouldQuit = true;

    if (!scriptFile.existsAsFile()) {
        appendResult(results, "script", false, "Script file does not exist: " + options.scriptPath);
    } else {
        var script = JSON::parse(scriptFile.loadFileAsString());
        var commandsValue = PresetJson::property(script, "commands");
        const Array<var>* commands = PresetJson::getArray(commandsValue);

        shouldQuit = getBool(script, "quit", true);

        if (commands == nullptr) {
            commands = PresetJson::getArray(script);
        }

        if (commands == nullptr) {
            appendResult(results, "script", false, "Script must be an array or an object with a commands array");
        } else {
            for (const auto& command : *commands) {
                runCommand(command, results);
            }
        }
    }

    auto report = PresetJson::object();
    report->setProperty("script", options.scriptPath);
    report->setProperty("results", var(results));
    report->setProperty("snapshot", snapshotState());

    if (options.reportPath.isNotEmpty()) {
        File reportFile(options.reportPath);
        reportFile.replaceWithText(JSON::toString(PresetJson::toVar(report), true));
    }

    if (shouldQuit) {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

void CycleAutomation::runCommand(const var& command, Array<var>& results) {
    String type = getString(command, "command", getString(command, "type"));
    String message;
    bool ok = false;

    if (type == "action") {
        ok = runTourAction(command, message);
    } else if (type == "screenshot") {
        ok = captureScreenshot(command, message);
    } else if (type == "exportState") {
        ok = exportState(command, message);
    } else if (type == "snapshotState") {
        ok = true;
        message = JSON::toString(snapshotState(), true);
    } else {
        message = "Unknown automation command: " + type;
    }

    appendResult(results, type, ok, message);
}

bool CycleAutomation::runTourAction(const var& command, String& message) {
    auto* object = PresetJson::getObject(command);

    if (object == nullptr) {
        message = "Action command must be an object";
        return false;
    }

    XmlElement actionElem("Action");
    const NamedValueSet& properties = object->getProperties();

    for (int i = 0; i < properties.size(); ++i) {
        Identifier name = properties.getName(i);

        String nameString = name.toString();

        if (nameString == "command") {
            continue;
        }

        if (nameString == "type" && properties.getValueAt(i).toString() == "action") {
            continue;
        }

        if (nameString == "actionType") {
            actionElem.setAttribute("type", properties.getValueAt(i).toString());
            continue;
        }

        actionElem.setAttribute(name.toString(), properties.getValueAt(i).toString());
    }

    CycleTour::Action action;
    getObj(CycleTour).readAction(action, &actionElem);
    getObj(CycleTour).performAction(action);

    message = "Action executed";
    return true;
}

bool CycleAutomation::captureScreenshot(const var& command, String& message) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "Screenshot command requires path";
        return false;
    }

    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Screenshot target could not be resolved";
        return false;
    }

    Image image = component->createComponentSnapshot(component->getLocalBounds());

    if (!image.isValid()) {
        message = "Snapshot image is invalid";
        return false;
    }

    File file(path);
    std::unique_ptr<FileOutputStream> stream(file.createOutputStream());

    if (stream == nullptr || !stream->openedOk()) {
        message = "Could not open screenshot path: " + path;
        return false;
    }

    PNGImageFormat format;

    if (!format.writeImageToStream(image, *stream)) {
        message = "Could not encode screenshot: " + path;
        return false;
    }

    message = "Screenshot written: " + path;
    return true;
}

bool CycleAutomation::exportState(const var& command, String& message) {
    String source = getString(command, "source", getString(command, "target"));
    String path = getString(command, "path");

    if (source.isEmpty()) {
        message = "Export command requires source";
        return false;
    }

    var exported = getObj(Document).exportSavableJSON(source);

    if (exported.isVoid()) {
        message = "No savable source found: " + source;
        return false;
    }

    String json = JSON::toString(exported, true);

    if (path.isNotEmpty()) {
        File(path).replaceWithText(json);
        message = "State exported: " + path;
    } else {
        message = json;
    }

    return true;
}

var CycleAutomation::snapshotState() {
    auto snapshot = PresetJson::object();

    snapshot->setProperty("document", getObj(Document).getDocumentName());
    snapshot->setProperty("automationRan", hasRun);

    return PresetJson::toVar(snapshot);
}

Component* CycleAutomation::resolveComponent(const var& command) {
    String area = getString(command, "area");
    String target = getString(command, "target");

    if (area.isNotEmpty()) {
        return getObj(CycleTour).getComponent(area, target);
    }

    return &getObj(MainPanel);
}
