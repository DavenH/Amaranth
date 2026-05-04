#include "CycleAutomation.h"

#include <utility>

#include <App/AutomationInspectable.h>
#include <App/Doc/Document.h>
#include <App/Doc/PresetJson.h>
#include <Definitions.h>
#include <UI/Panels/Panel.h>

#include "CycleTour.h"
#include "FileManager.h"

#include "../UI/Panels/MainPanel.h"

namespace {
    const char* const kInspectableAreas[] = {
        "AreaMain",
        "AreaWshpEditor",
        "AreaWfrmWaveform3D",
        "AreaSpectrum",
        "AreaSpectrogram",
        "AreaEnvelopes",
        "AreaMorphPanel",
        "AreaVertexProps",
        "AreaGenControls",
        "AreaPlayback",
        "AreaGuideCurves",
        "AreaImpulse",
        "AreaWaveshaper",
        "AreaUnison",
        "AreaReverb",
        "AreaDelay",
        "AreaEQ",
        "AreaModMatrix",
        "AreaMasterCtrls",
    };

    const char* const kTourGuideAreas[] = {
        "AreaWfrmWaveform3D",
        "AreaSpectrogram",
        "AreaEnvelopes",
        "AreaMorphPanel",
        "AreaVertexProps",
        "AreaGenControls",
        "AreaImpulse",
        "AreaWaveshaper",
        "AreaGuideCurves",
        "AreaUnison",
        "AreaModMatrix",
        "AreaMasterCtrls",
    };

    const char* const kInspectableTargets[] = {
        "TargMatrixGrid",
        "TargMatrixUtility",
        "TargMatrixSource",
        "TargMatrixDest",
        "TargMatrixAddDest",
        "TargMatrixAddSource",
        "TargUniMode",
        "TargUniVoiceSlct",
        "TargUniAddRemove",
        "TargDfrmNoise",
        "TargDfrmOffset",
        "TargDfrmPhase",
        "TargDfrmMeshSlct",
        "TargDfrmLayerAdd",
        "TargDfrmLayerSlct",
        "TargImpLength",
        "TargImpGain",
        "TargImpHP",
        "TargImpZoom",
        "TargImpLoadWav",
        "TargImpUnloadWav",
        "TargImpModelWav",
        "TargWaveshaperOvsp",
        "TargWaveshaperPre",
        "TargWaveshaperPost",
        "TargWaveshaperSlct",
        "TargSliderArea",
        "TargTimeSlider",
        "TargPhsSlider",
        "TargAmpSlider",
        "TargKeySlider",
        "TargMorphSlider",
        "TargCrvSlider",
        "TargBoxArea",
        "TargPhsBox",
        "TargAmpBox",
        "TargKeyBox",
        "TargModBox",
        "TargCrvBox",
        "TargAvpBox",
        "TargGainArea",
        "TargPhsGain",
        "TargAmpGain",
        "TargKeyGain",
        "TargModGain",
        "TargCrvGain",
        "TargAvpGain",
        "TargVol",
        "TargScratch",
        "TargPitch",
        "TargWavPitch",
        "TargScratchLyr",
        "TargSustLoop",
        "TargDomains",
        "TargLayerEnable",
        "TargLayerMode",
        "TargLayerAdder",
        "TargLayerMover",
        "TargLayerSlct",
        "TargScratchBox",
        "TargDeconv",
        "TargPhaseUp",
        "TargPan",
        "TargRange",
        "TargMeshSelector",
        "TargModelCycle",
        "TargSelector",
        "TargPencil",
        "TargAxe",
        "TargNudge",
        "TargWaveVerts",
        "TargVerts",
        "TargLinkYellow",
        "TargVertCube",
        "TargPrimeArea",
        "TargPrimeY",
        "TargPrimeB",
        "TargPrimeR",
        "TargLinkArea",
        "TargLinkY",
        "TargLinkB",
        "TargLinkR",
        "TargRangeArea",
        "TargRangeY",
        "TargRangeB",
        "TargRangeR",
        "TargSlidersArea",
        "TargSliderY",
        "TargSliderB",
        "TargSliderR",
        "TargSliderPan",
        "TargMasterVol",
        "TargMasterOct",
        "TargMasterLen",
    };

    String getString(const var& object, const Identifier& name, const String& fallback = {}) {
        return PresetJson::stringProperty(object, name, fallback);
    }

    bool getBool(const var& object, const Identifier& name, bool fallback = false) {
        return PresetJson::boolProperty(object, name, fallback);
    }

    double getDouble(const var& object, const Identifier& name, double fallback = 0.0) {
        return PresetJson::doubleProperty(object, name, fallback);
    }

    NotificationType getNotificationType(const var& object) {
        String notification = getString(object, "notification", "sync");

        if (notification == "none") {
            return dontSendNotification;
        }

        if (notification == "async") {
            return sendNotificationAsync;
        }

        return sendNotificationSync;
    }

    var makeResult(const String& type, bool ok, const String& message, const var& data = {}) {
        auto result = PresetJson::object();
        result->setProperty("type", type);
        result->setProperty("ok", ok);
        result->setProperty("message", message);

        if (!data.isVoid()) {
            result->setProperty("data", data);
        }

        return PresetJson::toVar(result);
    }

    var rectangleState(const Rectangle<int>& bounds) {
        auto json = PresetJson::object();
        json->setProperty("x", bounds.getX());
        json->setProperty("y", bounds.getY());
        json->setProperty("width", bounds.getWidth());
        json->setProperty("height", bounds.getHeight());
        return PresetJson::toVar(json);
    }

    bool getPathSegmentValue(const var& source, const String& segment, var& result) {
        String propertyName = segment;
        int bracketIndex = propertyName.indexOfChar('[');
        int arrayIndex = -1;

        if (bracketIndex >= 0 && propertyName.endsWithChar(']')) {
            String indexString = propertyName.substring(bracketIndex + 1, propertyName.length() - 1);
            propertyName = propertyName.substring(0, bracketIndex);
            arrayIndex = indexString.getIntValue();
        }

        result = propertyName.isEmpty() ? source : PresetJson::property(source, propertyName);

        if (result.isVoid()) {
            return false;
        }

        if (arrayIndex >= 0) {
            if (auto* values = result.getArray()) {
                if (isPositiveAndBelow(arrayIndex, values->size())) {
                    result = values->getReference(arrayIndex);
                    return true;
                }
            }

            return false;
        }

        return true;
    }

    bool getPathValue(const var& source, const String& path, var& result) {
        if (path.isEmpty()) {
            result = source;
            return true;
        }

        StringArray segments;
        segments.addTokens(path, ".", {});
        result = source;

        for (const auto& segment : segments) {
            if (!getPathSegmentValue(result, segment, result)) {
                return false;
            }
        }

        return true;
    }

    bool numbersEqual(double lhs, double rhs, double tolerance) {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool compareVars(const var& actual, const String& op, const var& expected, double tolerance) {
        if (op == "exists") {
            return !actual.isVoid();
        }

        if (op == "notEquals") {
            return !compareVars(actual, "equals", expected, tolerance);
        }

        if (op == "equals") {
            if (actual.isDouble() || actual.isInt() || actual.isInt64() ||
                expected.isDouble() || expected.isInt() || expected.isInt64()) {
                return numbersEqual(double(actual), double(expected), tolerance);
            }

            return actual.toString() == expected.toString();
        }

        double actualNumber = double(actual);
        double expectedNumber = double(expected);

        if (op == "lessThan") {
            return actualNumber < expectedNumber;
        }

        if (op == "lessThanOrEqual") {
            return actualNumber <= expectedNumber || numbersEqual(actualNumber, expectedNumber, tolerance);
        }

        if (op == "greaterThan") {
            return actualNumber > expectedNumber;
        }

        if (op == "greaterThanOrEqual") {
            return actualNumber >= expectedNumber || numbersEqual(actualNumber, expectedNumber, tolerance);
        }

        return false;
    }

    String assertionOperator(const var& command) {
        static const char* const operators[] = {
            "equals",
            "notEquals",
            "lessThan",
            "lessThanOrEqual",
            "greaterThan",
            "greaterThanOrEqual",
            "exists",
        };

        for (auto* op : operators) {
            if (!PresetJson::property(command, op).isVoid()) {
                return op;
            }
        }

        return {};
    }

    int getIdleDelayMs(const var& command) {
        double timeoutMs = getDouble(command, "timeoutMs", 50.0);
        return jlimit(0, 10000, int(getDouble(command, "idleDelayMs", getDouble(command, "delayMs", timeoutMs))));
    }

    bool drainMessageLoop(int delayMs) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            return MessageManager::getInstance()->runDispatchLoopUntil(delayMs);
        }

        Thread::sleep(delayMs);
        return false;
    }

    bool drainMessageLoopIfRequested(const var& command) {
        if (!getBool(command, "waitForIdle")) {
            return false;
        }

        drainMessageLoop(getIdleDelayMs(command));
        return true;
    }

    Point<float> getPointerPosition(const var& command, Component& component, const String& xName, const String& yName) {
        Rectangle<int> bounds = component.getLocalBounds();
        var xValue = PresetJson::property(command, xName);
        var yValue = PresetJson::property(command, yName);

        if (!xValue.isVoid() && !yValue.isVoid()) {
            return { float(double(xValue)), float(double(yValue)) };
        }

        var normalizedX = PresetJson::property(command, "normalizedX");
        var normalizedY = PresetJson::property(command, "normalizedY");

        if (!normalizedX.isVoid() && !normalizedY.isVoid()) {
            return {
                bounds.getWidth() * float(double(normalizedX)),
                bounds.getHeight() * float(double(normalizedY)),
            };
        }

        return bounds.getCentre().toFloat();
    }

    ModifierKeys getPointerModifiers(const var& command, bool buttonDown) {
        ModifierKeys modifiers = ModifierKeys::currentModifiers.withoutMouseButtons();

        if (buttonDown) {
            modifiers = modifiers.withFlags(ModifierKeys::leftButtonModifier);
        }

        if (getBool(command, "shift")) {
            modifiers = modifiers.withFlags(ModifierKeys::shiftModifier);
        }

        if (getBool(command, "command")) {
            modifiers = modifiers.withFlags(ModifierKeys::commandModifier);
        }

        if (getBool(command, "ctrl")) {
            modifiers = modifiers.withFlags(ModifierKeys::ctrlModifier);
        }

        if (getBool(command, "alt")) {
            modifiers = modifiers.withFlags(ModifierKeys::altModifier);
        }

        return modifiers;
    }

    MouseEvent makePointerEvent(Component& component,
                                Point<float> position,
                                Point<float> downPosition,
                                const var& command,
                                bool buttonDown,
                                bool wasDragged,
                                int clickCount) {
        Time now = Time::getCurrentTime();

        return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            getPointerModifiers(command, buttonDown),
            MouseInputSource::defaultPressure,
            0.0f, 0.0f, 0.0f, 0.0f,
            &component,
            &component,
            now,
            downPosition,
            now,
            clickCount,
            wasDragged,
        };
    }

    String componentRole(Component* component) {
        if (dynamic_cast<Slider*>(component) != nullptr) {
            return "slider";
        }

        if (dynamic_cast<Button*>(component) != nullptr) {
            return "button";
        }

        if (dynamic_cast<ComboBox*>(component) != nullptr) {
            return "comboBox";
        }

        if (dynamic_cast<TextEditor*>(component) != nullptr) {
            return "textEditor";
        }

        if (dynamic_cast<Label*>(component) != nullptr) {
            return "label";
        }

        if (dynamic_cast<Viewport*>(component) != nullptr) {
            return "viewport";
        }

        return "component";
    }

    String resolveSavableSourceName(const String& source) {
        static const std::pair<const char*, const char*> aliases[] = {
            { "meshLibrary",    "MeshLibrary" },
            { "morphPanel",     "MorphPanel" },
            { "mainPanel",      "MainPanel" },
            { "effects",        "EffectGuiRegistry" },
            { "modMatrix",      "ModMatrixPanel" },
            { "envelopes",      "Envelope2D" },
            { "envelopeProps",  "Envelope2D" },
            { "guideCurves",    "GuideCurvePanel" },
            { "oscControls",    "OscControlPanel" },
            { "settings",       "Settings" },
        };

        for (const auto& alias : aliases) {
            if (source == alias.first) {
                return alias.second;
            }
        }

        return source;
    }

    void setIfPathExists(DynamicObject& object, const Identifier& name, const var& source, const String& path) {
        var value;

        if (getPathValue(source, path, value)) {
            object.setProperty(name, value);
        }
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

void CycleAutomation::appendResult(Array<var>& results, const String& type, bool ok, const String& message, const var& data) {
    results.add(makeResult(type, ok, message, data));
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
    var data;
    bool ok = false;

    if (type == "action") {
        ok = runTourAction(command, message);
    } else if (type == "screenshot") {
        ok = captureScreenshot(command, message, data);
    } else if (type == "exportState") {
        ok = exportState(command, message);
    } else if (type == "exportPreset") {
        ok = exportPreset(command, message);
    } else if (type == "savePreset") {
        ok = savePreset(command, message, data);
    } else if (type == "openPreset") {
        ok = openPreset(command, message, data);
    } else if (type == "openFactoryPreset") {
        ok = openFactoryPreset(command, message, data);
    } else if (type == "inspectTargets") {
        ok = inspectTargets(command, message, data);
    } else if (type == "inspectTree") {
        ok = inspectTree(command, message, data);
    } else if (type == "setControl") {
        ok = setControl(command, message, data);
    } else if (type == "pointer") {
        ok = pointer(command, message, data);
    } else if (type == "assertTarget") {
        ok = assertTarget(command, message, data);
    } else if (type == "assertState") {
        ok = assertState(command, message, data);
    } else if (type == "waitForIdle") {
        ok = waitForIdle(command, message, data);
    } else if (type == "snapshotState") {
        ok = true;
        data = snapshotState();
        message = "Snapshot captured";
    } else {
        message = "Unknown automation command: " + type;
    }

    appendResult(results, type, ok, message, data);
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

bool CycleAutomation::captureScreenshot(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "Screenshot command requires path";
        return false;
    }

    bool waitedForIdle = drainMessageLoopIfRequested(command);
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

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("width", image.getWidth());
    json->setProperty("height", image.getHeight());
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));

    data = PresetJson::toVar(json);
    message = "Screenshot written: " + path;
    return true;
}

bool CycleAutomation::exportState(const var& command, String& message) {
    String requestedSource = getString(command, "source", getString(command, "target"));
    String source = resolveSavableSourceName(requestedSource);
    String jsonPath = getString(command, "jsonPath", getString(command, "statePath"));
    String path = getString(command, "path");

    if (requestedSource.isEmpty()) {
        message = "Export command requires source";
        return false;
    }

    var exported = getObj(Document).exportSavableJSON(source);

    if (exported.isVoid()) {
        message = "No savable source found: " + source;
        return false;
    }

    if (jsonPath.isNotEmpty()) {
        var selected;

        if (!getPathValue(exported, jsonPath, selected)) {
            message = "Export JSON path not found: " + jsonPath;
            return false;
        }

        exported = selected;
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

bool CycleAutomation::exportPreset(const var& command, String& message) {
    String path = getString(command, "path");
    String jsonPath = getString(command, "jsonPath", getString(command, "statePath"));
    var exported = getObj(Document).exportPresetJSON();

    if (jsonPath.isNotEmpty()) {
        var selected;

        if (!getPathValue(exported, jsonPath, selected)) {
            message = "Preset JSON path not found: " + jsonPath;
            return false;
        }

        exported = selected;
    }

    String json = JSON::toString(exported, true);

    if (path.isNotEmpty()) {
        File(path).replaceWithText(json);
        message = "Preset JSON exported: " + path;
    } else {
        message = json;
    }

    return true;
}

bool CycleAutomation::savePreset(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "savePreset requires path";
        return false;
    }

    File file(path);
    getObj(Document).save(path);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("exists", file.existsAsFile());
    json->setProperty("sizeBytes", file.existsAsFile() ? file.getSize() : 0);
    data = PresetJson::toVar(json);

    if (!file.existsAsFile()) {
        message = "Preset was not written: " + path;
        return false;
    }

    message = "Preset saved: " + path;
    return true;
}

bool CycleAutomation::openPreset(const var& command, String& message, var& data) {
    String path = getString(command, "path");

    if (path.isEmpty()) {
        message = "openPreset requires path";
        return false;
    }

    File file(path);

    if (!file.existsAsFile()) {
        message = "Preset file does not exist: " + path;
        return false;
    }

    bool opened = getObj(Document).open(path);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("opened", opened);
    json->setProperty("document", getObj(Document).getDocumentName());
    data = PresetJson::toVar(json);

    if (!opened) {
        message = "Preset could not be opened: " + path;
        return false;
    }

    message = "Preset opened: " + path;
    return true;
}

bool CycleAutomation::openFactoryPreset(const var& command, String& message, var& data) {
    String preset = getString(command, "preset", getString(command, "name"));

    if (preset.isEmpty()) {
        message = "openFactoryPreset requires preset";
        return false;
    }

    File file = getObj(FileManager).findFactoryPresetFile(preset);

    if (!file.existsAsFile()) {
        message = "Factory preset file does not exist: " + preset;
        return false;
    }

    getObj(FileManager).openFactoryPreset(preset);

    auto json = PresetJson::object();
    json->setProperty("preset", preset);
    json->setProperty("path", file.getFullPathName());
    json->setProperty("document", getObj(Document).getDocumentName());
    data = PresetJson::toVar(json);

    message = "Factory preset opened: " + preset;
    return true;
}

bool CycleAutomation::inspectTargets(const var& command, String& message, var& data) {
    String requestedArea = getString(command, "area");
    String requestedTarget = getString(command, "target");
    Array<var> targets;

    if (requestedArea.isNotEmpty()) {
        if (requestedTarget.isNotEmpty()) {
            targets.add(componentState(resolveComponent(command), requestedArea, requestedTarget));
        } else {
            targets.add(componentState(resolveComponent(command), requestedArea, {}));
        }
    } else {
        for (auto* area : kInspectableAreas) {
            targets.add(componentState(getObj(CycleTour).getComponent(area, {}), area, {}));
        }

        for (auto* area : kTourGuideAreas) {
            for (auto* target : kInspectableTargets) {
                Component* component = getObj(CycleTour).getComponent(area, target);

                if (component != nullptr) {
                    targets.add(componentState(component, area, target));
                }
            }
        }
    }

    auto json = PresetJson::object();
    json->setProperty("targets", var(targets));
    json->setProperty("count", targets.size());
    data = PresetJson::toVar(json);
    message = "Inspected " + String(targets.size()) + " targets";

    return true;
}

bool CycleAutomation::inspectTree(const var& command, String& message, var& data) {
    Component* root = resolveComponent(command);
    String requestedArea = getString(command, "area");

    if (root == nullptr) {
        message = "Tree root could not be resolved";
        return false;
    }

    int maxDepth = jlimit(0, 16, int(getDouble(command, "maxDepth", 5.0)));
    int maxNodes = jlimit(1, 5000, int(getDouble(command, "maxNodes", 500.0)));
    bool includeInvisible = getBool(command, "includeInvisible");
    int nodeCount = 0;

    auto json = PresetJson::object();
    json->setProperty("root", componentTreeState(root, "root", 0, maxDepth, maxNodes, nodeCount, includeInvisible));
    json->setProperty("registeredTargets", registeredTargetsForRoot(root, requestedArea));
    json->setProperty("count", nodeCount);
    json->setProperty("maxDepth", maxDepth);
    json->setProperty("maxNodes", maxNodes);
    json->setProperty("truncated", nodeCount >= maxNodes);

    data = PresetJson::toVar(json);
    message = "Inspected component tree with " + String(nodeCount) + " nodes";
    return true;
}

bool CycleAutomation::setControl(const var& command, String& message, var& data) {
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Control target could not be resolved";
        return false;
    }

    NotificationType notification = getNotificationType(command);

    if (auto* slider = dynamic_cast<Slider*>(component)) {
        var valueVar = PresetJson::property(command, "value");

        if (valueVar.isVoid()) {
            message = "Slider control requires value";
            return false;
        }

        slider->setValue(getDouble(command, "value"), notification);
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "Slider value set";
        return true;
    }

    if (auto* button = dynamic_cast<Button*>(component)) {
        if (getBool(command, "click")) {
            button->triggerClick();
        } else {
            var stateVar = PresetJson::property(command, "toggleState");

            if (stateVar.isVoid()) {
                stateVar = PresetJson::property(command, "value");
            }

            if (stateVar.isVoid()) {
                message = "Button control requires click, toggleState, or value";
                return false;
            }

            button->setToggleState(bool(stateVar), notification);
        }

        data = componentState(component, getString(command, "area"), getString(command, "target"));
        message = "Button control set";
        return true;
    }

    message = "Control target is not a supported Slider or Button";
    data = componentState(component, getString(command, "area"), getString(command, "target"));
    return false;
}

bool CycleAutomation::pointer(const var& command, String& message, var& data) {
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Pointer target could not be resolved";
        return false;
    }

    if (!component->isShowing()) {
        message = "Pointer target is not showing";
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    Rectangle<int> bounds = component->getLocalBounds();

    if (bounds.isEmpty()) {
        message = "Pointer target has empty bounds";
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    String eventType = getString(command, "event", getString(command, "pointerEvent", "click"));
    Point<float> position = getPointerPosition(command, *component, "x", "y");
    Point<float> downPosition = getPointerPosition(command, *component, "downX", "downY");
    bool waitedForIdle = drainMessageLoopIfRequested(command);

    if (eventType == "click") {
        component->mouseDown(makePointerEvent(*component, position, position, command, true, false, 1));
        component->mouseUp(makePointerEvent(*component, position, position, command, false, false, 1));
    } else if (eventType == "doubleClick") {
        component->mouseDoubleClick(makePointerEvent(*component, position, position, command, true, false, 2));
    } else if (eventType == "down") {
        component->mouseDown(makePointerEvent(*component, position, position, command, true, false, 1));
    } else if (eventType == "up") {
        component->mouseUp(makePointerEvent(*component, position, downPosition, command, false, false, 1));
    } else if (eventType == "drag") {
        component->mouseDrag(makePointerEvent(*component, position, downPosition, command, true, true, 1));
    } else if (eventType == "move") {
        component->mouseMove(makePointerEvent(*component, position, position, command, false, false, 0));
    } else if (eventType == "wheel") {
        MouseWheelDetails wheel {
            float(getDouble(command, "deltaX")),
            float(getDouble(command, "deltaY")),
            getBool(command, "reversed"),
            getBool(command, "smooth", true),
            getBool(command, "inertial"),
        };

        component->mouseWheelMove(makePointerEvent(*component, position, position, command, false, false, 0), wheel);
    } else {
        message = "Unknown pointer event: " + eventType;
        data = componentState(component, getString(command, "area"), getString(command, "target"));
        return false;
    }

    auto json = PresetJson::object();
    json->setProperty("event", eventType);
    json->setProperty("area", getString(command, "area"));
    json->setProperty("target", getString(command, "target"));
    json->setProperty("x", position.x);
    json->setProperty("y", position.y);
    json->setProperty("waitedForIdle", waitedForIdle);
    json->setProperty("localBounds", rectangleState(bounds));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));
    data = PresetJson::toVar(json);

    message = "Pointer event executed";
    return true;
}

bool CycleAutomation::assertTarget(const var& command, String& message, var& data) {
    String area = getString(command, "area");
    String target = getString(command, "target");
    String path = getString(command, "path");
    String op = assertionOperator(command);
    double tolerance = getDouble(command, "tolerance", 0.000001);
    drainMessageLoopIfRequested(command);
    Component* component = resolveComponent(command);

    if (component == nullptr) {
        message = "Assertion target could not be resolved";
        return false;
    }

    if (path.isEmpty()) {
        message = "assertTarget requires path";
        return false;
    }

    if (op.isEmpty()) {
        message = "assertTarget requires an assertion operator";
        return false;
    }

    var state = componentState(component, area, target);
    var actual;
    bool pathFound = getPathValue(state, path, actual);
    var expected = PresetJson::property(command, op);

    auto json = PresetJson::object();
    json->setProperty("area", area);
    json->setProperty("target", target);
    json->setProperty("path", path);
    json->setProperty("operator", op);
    json->setProperty("pathFound", pathFound);

    if (pathFound) {
        json->setProperty("actual", actual);
    }

    if (op != "exists") {
        json->setProperty("expected", expected);
    }

    data = PresetJson::toVar(json);

    if (!pathFound) {
        message = "Assertion path not found: " + path;
        return false;
    }

    if (!compareVars(actual, op, expected, tolerance)) {
        message = "Assertion failed for path " + path + ": actual=" + actual.toString();

        if (op != "exists") {
            message += " expected=" + expected.toString();
        }

        return false;
    }

    message = "Assertion passed";
    return true;
}

bool CycleAutomation::assertState(const var& command, String& message, var& data) {
    String path = getString(command, "path");
    String op = assertionOperator(command);
    double tolerance = getDouble(command, "tolerance", 0.000001);
    drainMessageLoopIfRequested(command);

    if (path.isEmpty()) {
        message = "assertState requires path";
        return false;
    }

    if (op.isEmpty()) {
        message = "assertState requires an assertion operator";
        return false;
    }

    var state = snapshotState();
    var actual;
    bool pathFound = getPathValue(state, path, actual);
    var expected = PresetJson::property(command, op);

    auto json = PresetJson::object();
    json->setProperty("path", path);
    json->setProperty("operator", op);
    json->setProperty("pathFound", pathFound);

    if (pathFound) {
        json->setProperty("actual", actual);
    }

    if (op != "exists") {
        json->setProperty("expected", expected);
    }

    data = PresetJson::toVar(json);

    if (!pathFound) {
        message = "Assertion path not found: " + path;
        return false;
    }

    if (!compareVars(actual, op, expected, tolerance)) {
        message = "Assertion failed for path " + path + ": actual=" + actual.toString();

        if (op != "exists") {
            message += " expected=" + expected.toString();
        }

        return false;
    }

    message = "Assertion passed";
    return true;
}

bool CycleAutomation::waitForIdle(const var& command, String& message, var& data) {
    int delayMs = getIdleDelayMs(command);
    bool dispatched = drainMessageLoop(delayMs);

    auto json = PresetJson::object();
    json->setProperty("delayMs", delayMs);
    json->setProperty("messageLoopDispatched", dispatched);
    data = PresetJson::toVar(json);

    message = "Idle wait completed";
    return true;
}

var CycleAutomation::snapshotState() {
    auto snapshot = PresetJson::object();
    auto panels = PresetJson::object();

    snapshot->setProperty("document", getObj(Document).getDocumentName());
    snapshot->setProperty("automationRan", hasRun);

    if (auto* controls = getObj(CycleTour).getAutomationInspectable("AreaGenControls", {})) {
        var state = controls->exportAutomationState();
        panels->setProperty("generalControls", state);
        setIfPathExists(*snapshot, "currentTool", state, "tools.name");
        setIfPathExists(*snapshot, "currentToolId", state, "tools.current");
        setIfPathExists(*snapshot, "viewStage", state, "viewStage");
        setIfPathExists(*snapshot, "viewStageName", state, "viewStageName");
    }

    if (auto* morphPanel = getObj(CycleTour).getAutomationInspectable("AreaMorphPanel", {})) {
        var state = morphPanel->exportAutomationState();
        panels->setProperty("morphPanel", state);
        setIfPathExists(*snapshot, "morphPosition", state, "sliders");
        setIfPathExists(*snapshot, "morphLinking", state, "linking");
        setIfPathExists(*snapshot, "morphRangeEnabled", state, "rangeEnabled");
    }

    if (auto* waveform3D = getObj(CycleTour).getAutomationInspectable("AreaWfrmWaveform3D", {})) {
        var state = waveform3D->exportAutomationState();
        panels->setProperty("waveform3D", state);
        setIfPathExists(*snapshot, "timeLayer", state, "layer");
    }

    snapshot->setProperty("panels", PresetJson::toVar(panels));

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

var CycleAutomation::componentState(Component* component, const String& area, const String& target) const {
    auto json = PresetJson::object();
    json->setProperty("area", area);
    json->setProperty("target", target);
    json->setProperty("resolved", component != nullptr);

    if (component != nullptr) {
        json->setProperty("name", component->getName());
        json->setProperty("class", String(typeid(*component).name()));
        json->setProperty("visible", component->isVisible());
        json->setProperty("showing", component->isShowing());
        json->setProperty("enabled", component->isEnabled());
        json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
        json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));

        if (auto* slider = dynamic_cast<Slider*>(component)) {
            json->setProperty("controlType", "slider");
            json->setProperty("value", slider->getValue());
            json->setProperty("minimum", slider->getMinimum());
            json->setProperty("maximum", slider->getMaximum());
            json->setProperty("interval", slider->getInterval());
            json->setProperty("text", slider->getTextFromValue(slider->getValue()));
        } else if (auto* button = dynamic_cast<Button*>(component)) {
            json->setProperty("controlType", "button");
            json->setProperty("toggleState", button->getToggleState());
            json->setProperty("buttonText", button->getButtonText());
        } else {
            json->setProperty("controlType", "component");
        }

        if (auto* inspectable = const_cast<CycleTour&>(getObj(CycleTour)).getAutomationInspectable(area, target)) {
            json->setProperty("automationState", inspectable->exportAutomationState());
        }
    } else {
        json->setProperty("error", "Target could not be resolved");
    }

    return PresetJson::toVar(json);
}

var CycleAutomation::registeredTargetsForRoot(Component* root, const String& requestedArea) const {
    Array<var> targets;
    Rectangle<int> rootBounds = root != nullptr ? root->getScreenBounds() : Rectangle<int>();
    CycleTour& tour = const_cast<CycleTour&>(getObj(CycleTour));

    if (root == nullptr) {
        return var(targets);
    }

    for (auto* area : kTourGuideAreas) {
        if (requestedArea.isNotEmpty() && requestedArea != area) {
            continue;
        }

        for (auto* target : kInspectableTargets) {
            Component* component = tour.getComponent(area, target);

            if (component == nullptr) {
                continue;
            }

            Rectangle<int> targetBounds = component->getScreenBounds();

            if (component == root || root->isParentOf(component) || rootBounds.intersects(targetBounds)) {
                targets.add(componentState(component, area, target));
            }
        }
    }

    return var(targets);
}

var CycleAutomation::componentTreeState(Component* component,
                                        const String& path,
                                        int depth,
                                        int maxDepth,
                                        int maxNodes,
                                        int& nodeCount,
                                        bool includeInvisible) const {
    auto json = PresetJson::object();

    if (component == nullptr || nodeCount >= maxNodes) {
        json->setProperty("truncated", true);
        return PresetJson::toVar(json);
    }

    ++nodeCount;

    json->setProperty("path", path);
    json->setProperty("depth", depth);
    json->setProperty("name", component->getName());
    json->setProperty("class", String(typeid(*component).name()));
    json->setProperty("role", componentRole(component));
    json->setProperty("visible", component->isVisible());
    json->setProperty("showing", component->isShowing());
    json->setProperty("enabled", component->isEnabled());
    json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));
    annotateTourTarget(*json, component);

    if (auto* slider = dynamic_cast<Slider*>(component)) {
        json->setProperty("value", slider->getValue());
        json->setProperty("minimum", slider->getMinimum());
        json->setProperty("maximum", slider->getMaximum());
        json->setProperty("text", slider->getTextFromValue(slider->getValue()));
    } else if (auto* button = dynamic_cast<Button*>(component)) {
        json->setProperty("toggleState", button->getToggleState());
        json->setProperty("buttonText", button->getButtonText());
    }

    if (depth >= maxDepth || nodeCount >= maxNodes) {
        json->setProperty("childrenTruncated", component->getNumChildComponents() > 0);
        return PresetJson::toVar(json);
    }

    Array<var> children;

    for (int i = 0; i < component->getNumChildComponents() && nodeCount < maxNodes; ++i) {
        Component* child = component->getChildComponent(i);

        if (child == nullptr) {
            continue;
        }

        if (!includeInvisible && !child->isVisible()) {
            continue;
        }

        children.add(componentTreeState(child, path + "/" + String(i), depth + 1, maxDepth, maxNodes, nodeCount, includeInvisible));
    }

    json->setProperty("children", var(children));
    return PresetJson::toVar(json);
}

void CycleAutomation::annotateTourTarget(DynamicObject& json, Component* component) const {
    CycleTour& tour = const_cast<CycleTour&>(getObj(CycleTour));

    for (auto* area : kInspectableAreas) {
        if (tour.getComponent(area, {}) == component) {
            json.setProperty("area", area);
            break;
        }
    }

    for (auto* area : kTourGuideAreas) {
        for (auto* target : kInspectableTargets) {
            if (tour.getComponent(area, target) == component) {
                json.setProperty("area", area);
                json.setProperty("target", target);
                return;
            }
        }
    }
}
