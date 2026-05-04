#include "CycleAutomation.h"

#include <App/Doc/Document.h>
#include <App/Doc/PresetJson.h>
#include <Definitions.h>
#include <UI/Panels/Panel.h>

#include "CycleTour.h"

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
    } else if (type == "inspectTargets") {
        ok = inspectTargets(command, message, data);
    } else if (type == "setControl") {
        ok = setControl(command, message, data);
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
    json->setProperty("localBounds", rectangleState(component->getLocalBounds()));
    json->setProperty("screenBounds", rectangleState(component->getScreenBounds()));

    data = PresetJson::toVar(json);
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
    } else {
        json->setProperty("error", "Target could not be resolved");
    }

    return PresetJson::toVar(json);
}
