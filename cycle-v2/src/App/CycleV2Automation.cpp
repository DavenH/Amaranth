#include "App/CycleV2Automation.h"

#include "App/CycleV2AutomationAssertions.h"
#include "App/CycleV2AutomationCommand.h"
#include "App/CycleV2AutomationInput.h"
#include "App/CycleV2AutomationProtocol.h"
#include "App/CycleV2AutomationSessionTransport.h"
#include "App/CycleV2AutomationWorkspaceCommands.h"
#include "App/OfflineAudioCaptureAutomation.h"
#include "UI/NodeWorkspace.h"

#include <utility>

namespace CycleV2 {

using namespace AutomationProtocol;

namespace {

var menuItemToVar(const String& id, const String& menu, const String& label, bool requiresPath) {
    var item = makeObject();
    auto* object = objectFor(item);
    object->setProperty("id", id);
    object->setProperty("menu", menu);
    object->setProperty("label", label);
    object->setProperty("requiresPath", requiresPath);
    return item;
}

var paletteItemToVar(const String& id, const String& section, const String& label) {
    var item = makeObject();
    auto* object = objectFor(item);
    object->setProperty("id", id);
    object->setProperty("section", section);
    object->setProperty("label", label);
    return item;
}

bool checkMetricThreshold(
        const var& command,
        const var& metrics,
        const String& commandProperty,
        const String& metricProperty,
        const String& op,
        String& message) {
    const auto* commandObject = objectFor(command);
    const auto* metricsObject = objectFor(metrics);

    if (commandObject == nullptr || metricsObject == nullptr) {
        return true;
    }

    const var expected = commandObject->getProperty(Identifier(commandProperty));

    if (expected.isVoid()) {
        return true;
    }

    const var actual = metricsObject->getProperty(Identifier(metricProperty));

    if (compareValues(actual, op, expected)) {
        return true;
    }

    message = "Audio assertion failed: " + metricProperty + " " + actual.toString()
            + " was not " + op + " " + expected.toString();
    return false;
}

bool checkAudioThresholds(const var& command, const var& metrics, String& message) {
    return checkMetricThreshold(command, metrics, "peakGreaterThan", "peak", "greaterThan", message)
            && checkMetricThreshold(command, metrics, "peakLessThan", "peak", "lessThan", message)
            && checkMetricThreshold(command, metrics, "rmsGreaterThan", "rms", "greaterThan", message)
            && checkMetricThreshold(command, metrics, "rmsLessThan", "rms", "lessThan", message);
}

}

CycleV2Automation::Options CycleV2Automation::parseCommandLine(const String& commandLine) {
    Options options;
    const StringArray tokens = StringArray::fromTokens(commandLine, true);

    for (int i = 0; i < tokens.size(); ++i) {
        const String token = tokens[i];

        if (token == "--agent-script" && i + 1 < tokens.size()) {
            options.scriptFile = File(tokens[++i].unquoted());
        } else if (token.startsWith("--agent-script=")) {
            options.scriptFile = File(token.fromFirstOccurrenceOf("=", false, false).unquoted());
        } else if (token == "--agent-report" && i + 1 < tokens.size()) {
            options.reportFile = File(tokens[++i].unquoted());
        } else if (token.startsWith("--agent-report=")) {
            options.reportFile = File(token.fromFirstOccurrenceOf("=", false, false).unquoted());
        } else if (token == "--agent-session" && i + 1 < tokens.size()) {
            options.hasSession = true;
            options.sessionPath = tokens[++i].unquoted();
        } else if (token.startsWith("--agent-session=")) {
            options.hasSession = true;
            options.sessionPath = token.fromFirstOccurrenceOf("=", false, false).unquoted();
        }
    }

    return options;
}

bool CycleV2Automation::hasAutomation(const Options& options) {
    return options.scriptFile != File() || options.reportFile != File() || options.hasSession;
}

CycleV2Automation::CycleV2Automation(NodeWorkspace& workspace, Component& window, Options options) :
        workspace    (workspace)
    ,   window       (window)
    ,   options      (std::move(options)) {
    assertions = std::make_unique<CycleV2AutomationAssertions>(
            [this]() { return snapshotState(); },
            [this](const String& nodeId, const String& parameterId, String& value) {
                return this->workspace.getNodeParameterForAutomation(
                        nodeId, parameterId, value);
            });
    workspaceCommands = std::make_unique<CycleV2AutomationWorkspaceCommands>(
            workspace,
            [this]() { return snapshotState(); },
            [this](const String& path) { return resolveCommandPath(path); });
    sessionTransport = std::make_unique<CycleV2AutomationSessionTransport>(
            [this](const var& command) { return runCommand(command); });
    input = std::make_unique<CycleV2AutomationInput>(
            workspace,
            [this](const String& area) { return componentForArea(area); },
            CycleV2AutomationInput::SemanticHandlers {
                    [this](const var& command) { return workspaceCommands->setMorphSlider(command); },
                    [this](const var& command) { return workspaceCommands->setPrimaryAxis(command); },
                    [this](const var& command) { return workspaceCommands->toggleLink(command); },
                    [this](const var& command) { return workspaceCommands->setVertexParameter(command); }
            });
}

CycleV2Automation::~CycleV2Automation() {
    sessionTransport = nullptr;
}

File CycleV2Automation::resolveCommandPath(const String& path) const {
    const File file(path);

    if (path.isEmpty() || File::isAbsolutePath(path)) {
        return file;
    }
    if (options.scriptFile != File()) {
        return options.scriptFile.getParentDirectory().getChildFile(path);
    }

    return File::getCurrentWorkingDirectory().getChildFile(path);
}

void CycleV2Automation::runScriptAsync() {
    MessageManager::callAsync([safeThis = Component::SafePointer<Component>(&window), this]() {
        if (safeThis == nullptr) {
            return;
        }

        if (options.hasSession) {
            startSessionServer();
        }

        if (options.scriptFile == File()) {
            return;
        }

        var report = makeObject();
        auto* reportObject = objectFor(report);
        reportObject->setProperty("script", options.scriptFile.getFullPathName());
        reportObject->setProperty("schema", "cycle-v2-agent-report.v1");

        Array<var> results;
        bool shouldQuit = false;

        if (!options.scriptFile.existsAsFile()) {
            results.add(failedResult("loadScript", "Automation script not found: " + options.scriptFile.getFullPathName()));
        } else {
            const var script = JSON::parse(options.scriptFile.loadFileAsString());
            Array<var>* commands = nullptr;

            if (auto* array = script.getArray()) {
                commands = array;
            } else if (const auto* scriptObject = objectFor(script)) {
                shouldQuit = (bool) scriptObject->getProperty("quit");
                commands = scriptObject->getProperty("commands").getArray();
            }

            if (commands == nullptr) {
                results.add(failedResult("loadScript", "Automation script must be an array or object with commands array"));
            } else {
                for (const auto& commandValue : *commands) {
                    const var result = runCommand(commandValue);
                    results.add(result);

                    if (stringProperty(commandValue, "command") == "quit") {
                        shouldQuit = true;
                    }
                }
            }
        }

        reportObject->setProperty("results", results);
        reportObject->setProperty("snapshot", snapshotState());
        writeReport(report);

        if (shouldQuit) {
            JUCEApplicationBase::quit();
        }
    });
}

void CycleV2Automation::startSessionServer() {
    if (sessionTransport->isRunning() || !options.hasSession) {
        return;
    }

    if (options.sessionPath.isEmpty()) {
        DBG("Cycle V2 automation session path is empty");
        return;
    }

    String message;
    if (!sessionTransport->start(options.sessionPath, message)) {
        DBG(message);
        return;
    }

    DBG(message);
}

var CycleV2Automation::runCommand(const var& commandValue) {
    const String command = stringProperty(commandValue, "command");
    const auto registered = automationCommandForName(command);
    if (!registered.has_value()) {
        return failedResult(
                command.isEmpty() ? "unknown" : command,
                "Unknown Cycle V2 automation command");
    }

    using Command = CycleV2AutomationCommand;
    switch (*registered) {
        case Command::SnapshotState:
            return okResult(command, snapshotState());
        case Command::InspectTargets:
            return inspectTargets(commandValue);
        case Command::InspectPointerTargets:
            return inspectPointerTargets();
        case Command::InspectPointerCursor:
            return inspectPointerCursor();
        case Command::InspectOpenGLDiagnostics:
            return inspectOpenGLDiagnostics();
        case Command::InspectCanvasPerformance:
            return inspectCanvasPerformance();
        case Command::ResetCanvasPerformance:
            return resetCanvasPerformance();
        case Command::InspectAudioPerformance:
            return inspectAudioPerformance();
        case Command::ResetAudioPerformance:
            return resetAudioPerformance();
        case Command::SendMidi:
            return sendMidi(commandValue);
        case Command::RequestCanvasOpenGLFrame:
            return requestCanvasOpenGLFrame();
        case Command::ExportGraph:
            return exportGraph(commandValue);
        case Command::OpenGraph:
            return openGraph(commandValue);
        case Command::SaveGraph:
            return saveGraph(commandValue);
        case Command::GeneratePresetPreview:
            return generatePresetPreview(commandValue);
        case Command::ListMenuItems:
            return listMenuItems();
        case Command::InvokeMenuItem:
            return invokeMenuItem(commandValue);
        case Command::ListPaletteItems:
            return listPaletteItems();
        case Command::InvokePaletteItem:
            return invokePaletteItem(commandValue);
        case Command::CaptureAudio:
            return captureAudio(commandValue);
        case Command::CaptureLiveAudio:
            return captureLiveAudio(commandValue);
        case Command::OpenNodeEditor:
            return workspaceCommands->openNodeEditor(commandValue);
        case Command::AddNode:
            return workspaceCommands->addNode(commandValue);
        case Command::MoveNode:
            return workspaceCommands->moveNode(commandValue);
        case Command::ConnectPorts:
            return workspaceCommands->connectPorts(commandValue);
        case Command::DeleteNode:
            return workspaceCommands->deleteNode(commandValue);
        case Command::DeleteEdge:
            return workspaceCommands->deleteEdge(commandValue);
        case Command::DeleteGuideCurve:
            return workspaceCommands->deleteGuideCurve(commandValue);
        case Command::LoadGuideHeatmap:
            return workspaceCommands->loadGuideHeatmap(commandValue);
        case Command::ClearGuideHeatmap:
            return workspaceCommands->clearGuideHeatmap(commandValue);
        case Command::Undo:
            return workspaceCommands->undo();
        case Command::SetNodeParameter:
            return workspaceCommands->setNodeParameter(commandValue);
        case Command::SetGuideParameter:
            return workspaceCommands->setGuideParameter(commandValue);
        case Command::InspectNodeControls:
            return workspaceCommands->inspectNodeControls(commandValue);
        case Command::SetMorphSlider:
            return workspaceCommands->setMorphSlider(commandValue);
        case Command::SetPrimaryAxis:
            return workspaceCommands->setPrimaryAxis(commandValue);
        case Command::ToggleLink:
            return workspaceCommands->toggleLink(commandValue);
        case Command::SelectVertex:
            return workspaceCommands->selectVertex(commandValue);
        case Command::SetVertexParameter:
            return workspaceCommands->setVertexParameter(commandValue);
        case Command::Pointer:
            return input->pointer(commandValue);
        case Command::Key:
            return input->key(commandValue);
        case Command::Screenshot:
            return screenshot(commandValue);
        case Command::AssertState:
            return assertions->assertState(commandValue);
        case Command::AssertNodeParameter:
            return assertions->assertNodeParameter(commandValue);
        case Command::ListAssertionPaths:
            return assertions->listAssertionPaths();
        case Command::WaitForIdle:
            return waitForIdle(commandValue);
        case Command::Quit:
            return okResult(command);
    }

    jassertfalse;
    return failedResult(command, "Unregistered Cycle V2 automation command");
}

var CycleV2Automation::snapshotState() const {
    var state = workspace.exportAutomationState();

    if (auto* object = objectFor(state)) {
        object->setProperty("windowTitle", window.getName());
        object->setProperty("windowBounds", rectangleToVar(window.getBounds()));
        object->setProperty("workspaceBounds", rectangleToVar(workspace.getBounds()));
        object->setProperty("canvasBounds", rectangleToVar(workspace.getCanvas().getBounds()));
    }

    return state;
}

var CycleV2Automation::inspectTargets(const var& commandValue) const {
    const String requestedArea = stringProperty(commandValue, "area");
    Array<var> targets;

    auto addArea = [&](const String& area) {
        if (Component* component = componentForArea(area)) {
            targets.add(componentInfo(area, *component));
        } else {
            var target = makeObject();
            auto* object = objectFor(target);
            object->setProperty("area", area);
            object->setProperty("target", "");
            object->setProperty("resolved", false);
            object->setProperty("error", "Unknown area");
            targets.add(target);
        }
    };

    if (requestedArea.isNotEmpty()) {
        addArea(requestedArea);
    } else {
        addArea("window");
        addArea("workspace");
        addArea("canvas");
    }

    var data = makeObject();
    objectFor(data)->setProperty("targets", targets);
    return okResult("inspectTargets", data);
}

var CycleV2Automation::exportGraph(const var& commandValue) const {
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));
    const String json = workspace.exportGraphJson();

    if (path == File()) {
        var data = makeObject();
        objectFor(data)->setProperty("json", json);
        return okResult("exportGraph", data);
    }

    path.getParentDirectory().createDirectory();
    if (!path.replaceWithText(json)) {
        return failedResult("exportGraph", "Could not write graph: " + path.getFullPathName());
    }

    var data = makeObject();
    objectFor(data)->setProperty("path", path.getFullPathName());
    return okResult("exportGraph", data);
}

var CycleV2Automation::openGraph(const var& commandValue) {
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));

    if (!workspace.loadGraphFromFile(path)) {
        return failedResult("openGraph", "Could not open graph: " + path.getFullPathName());
    }

    return okResult("openGraph", snapshotState());
}

var CycleV2Automation::saveGraph(const var& commandValue) {
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));

    if (!workspace.saveGraphToFile(path)) {
        return failedResult("saveGraph", "Could not save graph: " + path.getFullPathName());
    }

    var data = makeObject();
    objectFor(data)->setProperty("path", path.getFullPathName());
    return okResult("saveGraph", data);
}

var CycleV2Automation::generatePresetPreview(const var& commandValue) {
    const String viewId = stringProperty(commandValue, "view", "spectrum");
    const auto view = presetPreviewViewForId(viewId);
    if (!view.has_value()) {
        return failedResult(
                "generatePresetPreview",
                "Unknown preview view: " + viewId);
    }

    const String path = stringProperty(commandValue, "path");
    const File destination = path.isEmpty() ? File() : resolveCommandPath(path);
    const bool embed = boolProperty(commandValue, "embed", true);
    String errorMessage;
    PresetPreviewImage image;
    if (!workspace.capturePresetPreviewForAutomation(
                *view,
                image,
                errorMessage)) {
        return failedResult("generatePresetPreview", errorMessage);
    }
    const MemoryBlock encoded = image.jpegData;
    const int width = image.width;
    const int height = image.height;
    if (embed && !workspace.savePresetPreviewForAutomation(
                std::move(image), destination, errorMessage)) {
        return failedResult("generatePresetPreview", errorMessage);
    }

    var data = makeObject();
    if (embed) {
        objectFor(data)->setProperty(
                "path",
                (destination == File() ? workspace.graphFile() : destination).getFullPathName());
    }
    objectFor(data)->setProperty("view", idForPresetPreviewView(*view));
    objectFor(data)->setProperty("width", width);
    objectFor(data)->setProperty("height", height);
    objectFor(data)->setProperty("mediaType", "image/jpeg");
    objectFor(data)->setProperty("data", Base64::toBase64(
            encoded.getData(), encoded.getSize()));
    return okResult("generatePresetPreview", data);
}

var CycleV2Automation::listMenuItems() const {
    Array<var> items;
    items.add(menuItemToVar("file.openGraph", "File", "Open Preset...", true));
    items.add(menuItemToVar(
            "file.saveGraph",
            "File",
            "Save Preset",
            workspace.isGraphDirty()));
    items.add(menuItemToVar("file.saveGraphAs", "File", "Save Preset As...", true));
    items.add(menuItemToVar(
            "file.spyRefreshOnRelease",
            "File > Spy Refresh",
            "On Release",
            true));
    items.add(menuItemToVar(
            "file.spyRefreshLive",
            "File > Spy Refresh",
            "Live",
            true));

    var data = makeObject();
    objectFor(data)->setProperty("items", items);
    return okResult("listMenuItems", data);
}

var CycleV2Automation::invokeMenuItem(const var& commandValue) {
    const String id = stringProperty(commandValue, "id", stringProperty(commandValue, "itemId"));

    if (id == "file.openGraph") {
        return openGraph(commandValue);
    }
    if (id == "file.saveGraph" || id == "file.saveGraphAs") {
        return saveGraph(commandValue);
    }
    if (id == "file.spyRefreshOnRelease") {
        workspace.setProbeRefreshMode(ProbeRefreshMode::OnGestureCommit);
        return okResult("invokeMenuItem", snapshotState());
    }
    if (id == "file.spyRefreshLive") {
        workspace.setProbeRefreshMode(ProbeRefreshMode::LiveLatest);
        return okResult("invokeMenuItem", snapshotState());
    }

    return failedResult("invokeMenuItem", "Unknown menu item: " + id);
}

var CycleV2Automation::listPaletteItems() const {
    Array<var> items;
    items.add(paletteItemToVar("voiceContext", "Context", "Voice Context"));
    items.add(paletteItemToVar("fft", "Transform", "Time -> Freq"));
    items.add(paletteItemToVar("ifft", "Transform", "Freq -> Time"));
    items.add(paletteItemToVar("add", "Math", "Add"));
    items.add(paletteItemToVar("multiply", "Math", "Multiply"));
    items.add(paletteItemToVar("waveSource", "Source", "Wave"));
    items.add(paletteItemToVar("imageSource", "Source", "Image"));
    items.add(paletteItemToVar("trilinearMesh", "Source", "Mesh"));
    items.add(paletteItemToVar("envelope", "Control", "Envelope"));
    items.add(paletteItemToVar("impulseResponse", "FX", "IR"));
    items.add(paletteItemToVar("waveshaper", "FX", "Waveshaper"));
    items.add(paletteItemToVar("reverb", "FX", "Reverb"));
    items.add(paletteItemToVar("delay", "FX", "Delay"));
    items.add(paletteItemToVar("stereoSplit", "Channel", "Split"));
    items.add(paletteItemToVar("stereoJoin", "Channel", "Join"));
    items.add(paletteItemToVar("output", "Channel", "Output"));

    var data = makeObject();
    objectFor(data)->setProperty("items", items);
    return okResult("listPaletteItems", data);
}

var CycleV2Automation::invokePaletteItem(const var& commandValue) {
    const String id = stringProperty(commandValue, "id", stringProperty(commandValue, "itemId", stringProperty(commandValue, "kind")));

    if (id.isEmpty()) {
        return failedResult("invokePaletteItem", "Missing palette item id");
    }

    var addCommand = makeObject();
    auto* object = objectFor(addCommand);
    object->setProperty("command", "addNode");
    object->setProperty("kind", id);
    object->setProperty("x", floatProperty(commandValue, "x", 0.f));
    object->setProperty("y", floatProperty(commandValue, "y", 0.f));
    return workspaceCommands->addNode(addCommand);
}

var CycleV2Automation::captureAudio(const var& commandValue) {
    if (OfflineAudioCaptureAutomation::isScheduledCapture(commandValue)) {
        GraphExecutionPlan plan;
        uint64_t revision {};
        if (!workspace.copyAudioPlanForAutomation(plan, revision)) {
            return failedResult("captureAudio", "Cannot capture audio from an uncompiled graph");
        }

        const File path = resolveCommandPath(stringProperty(commandValue, "path"));
        var data;
        String error;
        if (!OfflineAudioCaptureAutomation::capture(
                commandValue,
                path,
                std::move(plan),
                revision,
                data,
                error)) {
            return failedResult("captureAudio", error);
        }

        String message;
        if (!checkAudioThresholds(commandValue, data, message)) {
            var result = failedResult("captureAudio", message);
            objectFor(result)->setProperty("data", data);
            return result;
        }
        return okResult("captureAudio", data);
    }

    const int frameCount = jlimit(1, 262144, intProperty(commandValue, "frames", intProperty(commandValue, "samples", 4096)));
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));
    var data = workspace.captureAudioForAutomation((size_t) frameCount);
    const auto* dataObject = objectFor(data);

    if (dataObject == nullptr || !(bool) dataObject->getProperty("compileSucceeded")) {
        return failedResult("captureAudio", "Cannot capture audio from an uncompiled graph");
    }
    if (!(bool) dataObject->getProperty("finite")) {
        var result = failedResult("captureAudio", "Audio capture contains non-finite samples");
        objectFor(result)->setProperty("data", data);
        return result;
    }

    const var metrics = dataObject->getProperty("metrics");
    String message;

    if (!checkAudioThresholds(commandValue, metrics, message)) {
        var result = failedResult("captureAudio", message);
        objectFor(result)->setProperty("data", data);
        return result;
    }

    if (path != File()) {
        const var samplesValue = dataObject->getProperty("samples");
        const Array<var>* samples = samplesValue.getArray();

        if (samples == nullptr) {
            return failedResult("captureAudio", "Audio capture did not include samples");
        }

        AudioSampleBuffer buffer(1, samples->size());
        for (int i = 0; i < samples->size(); ++i) {
            buffer.setSample(0, i, (float) (double) samples->getReference(i));
        }

        path.getParentDirectory().createDirectory();
        path.deleteFile();
        std::unique_ptr<FileOutputStream> stream(path.createOutputStream());

        if (stream == nullptr || !stream->openedOk()) {
            return failedResult("captureAudio", "Could not open audio capture path: " + path.getFullPathName());
        }

        WavAudioFormat wavFormat;
        std::unique_ptr<AudioFormatWriter> writer(
                wavFormat.createWriterFor(stream.get(), 44100.0, 1, 24, {}, 0));

        if (writer == nullptr) {
            return failedResult("captureAudio", "Could not create WAV writer: " + path.getFullPathName());
        }

        stream.release();

        if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
            return failedResult("captureAudio", "Could not write WAV capture: " + path.getFullPathName());
        }

        objectFor(data)->setProperty("path", path.getFullPathName());
    }

    return okResult("captureAudio", data);
}

var CycleV2Automation::captureLiveAudio(const var& commandValue) {
    const int durationMs = jlimit(10, 1400, intProperty(commandValue, "durationMs", 500));
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));
    auto capture = workspace.captureLiveAudioForAutomation(durationMs);
    if (!capture.completed || capture.left.empty() || capture.right.empty()) {
        return failedResult("captureLiveAudio", "Live audio-device capture did not complete");
    }

    var metrics = makeObject();
    auto* metricsObject = objectFor(metrics);
    metricsObject->setProperty("peak", capture.peak);
    metricsObject->setProperty("rms", capture.rms);
    String message;
    if (!checkAudioThresholds(commandValue, metrics, message)) {
        return failedResult("captureLiveAudio", message);
    }

    if (path != File()) {
        AudioSampleBuffer buffer(2, (int) capture.left.size());
        buffer.copyFrom(0, 0, capture.left.data(), (int) capture.left.size());
        buffer.copyFrom(1, 0, capture.right.data(), (int) capture.right.size());
        path.getParentDirectory().createDirectory();
        path.deleteFile();
        std::unique_ptr<FileOutputStream> stream(path.createOutputStream());
        if (stream == nullptr || !stream->openedOk()) {
            return failedResult("captureLiveAudio", "Could not open live capture path: " + path.getFullPathName());
        }
        WavAudioFormat wavFormat;
        std::unique_ptr<AudioFormatWriter> writer(
                wavFormat.createWriterFor(stream.get(), capture.sampleRate, 2, 24, {}, 0));
        if (writer == nullptr) {
            return failedResult("captureLiveAudio", "Could not create live WAV writer");
        }
        stream.release();
        if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
            return failedResult("captureLiveAudio", "Could not write live WAV capture");
        }
    }

    var data = makeObject();
    auto* dataObject = objectFor(data);
    dataObject->setProperty("source", "audioDeviceCallback");
    dataObject->setProperty("sampleRate", capture.sampleRate);
    dataObject->setProperty("frameCount", (int) capture.left.size());
    dataObject->setProperty("firstCallback", (int64) capture.firstCallback);
    dataObject->setProperty("lastCallback", (int64) capture.lastCallback);
    dataObject->setProperty("metrics", metrics);
    if (path != File()) {
        dataObject->setProperty("path", path.getFullPathName());
    }
    return okResult("captureLiveAudio", data);
}

var CycleV2Automation::inspectPointerTargets() const {
    return okResult("inspectPointerTargets", workspace.inspectPointerTargetsForAutomation());
}

var CycleV2Automation::inspectPointerCursor() const {
    const auto source = Desktop::getInstance().getMainMouseSource();
    Component* component = source.getComponentUnderMouse();
    const Point<int> screenPosition = Desktop::getMousePosition();
    if (component == nullptr) {
        const Point<int> windowPosition = window.getLocalPoint(nullptr, screenPosition);
        component = window.getComponentAt(windowPosition);
    }
    if (component == nullptr) {
        return failedResult(
                "inspectPointerCursor",
                "No component is under the OS pointer at "
                        + String(screenPosition.x) + "," + String(screenPosition.y));
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("component", component->getName());
    object->setProperty("cursor", cursorName(component->getMouseCursor()));
    object->setProperty("screenX", screenPosition.x);
    object->setProperty("screenY", screenPosition.y);
    return okResult("inspectPointerCursor", data);
}

var CycleV2Automation::inspectOpenGLDiagnostics() const {
    return okResult("inspectOpenGLDiagnostics", workspace.inspectOpenGLDiagnosticsForAutomation());
}

var CycleV2Automation::inspectCanvasPerformance() const {
    return okResult(
            "inspectCanvasPerformance",
            workspace.inspectCanvasPerformanceForAutomation());
}

var CycleV2Automation::resetCanvasPerformance() {
    workspace.resetCanvasPerformanceForAutomation();
    return okResult("resetCanvasPerformance");
}

var CycleV2Automation::inspectAudioPerformance() const {
    return okResult(
            "inspectAudioPerformance",
            workspace.inspectAudioPerformanceForAutomation());
}

var CycleV2Automation::resetAudioPerformance() {
    workspace.resetAudioPerformanceForAutomation();
    return okResult("resetAudioPerformance");
}

var CycleV2Automation::sendMidi(const var& commandValue) {
    const String type = stringProperty(commandValue, "type");
    const int channel = jlimit(1, 16, intProperty(commandValue, "channel", 1));
    const int note = jlimit(0, 127, intProperty(commandValue, "note", 60));
    MidiMessage message;
    if (type == "noteOn") {
        const float velocity = jlimit(
                0.f,
                1.f,
                floatProperty(commandValue, "velocity", 0.8f));
        message = MidiMessage::noteOn(channel, note, velocity);
    } else if (type == "noteOff") {
        message = MidiMessage::noteOff(channel, note);
    } else if (type == "allNotesOff") {
        message = MidiMessage::allNotesOff(channel);
    } else {
        return failedResult("sendMidi", "Unknown MIDI event type: " + type);
    }

    if (!workspace.enqueueMidiForAutomation(message)) {
        return failedResult("sendMidi", "The realtime MIDI queue is full");
    }
    return okResult("sendMidi");
}

var CycleV2Automation::requestCanvasOpenGLFrame() {
    workspace.requestCanvasOpenGLFrameForAutomation();
    return okResult("requestCanvasOpenGLFrame");
}

var CycleV2Automation::screenshot(const var& commandValue) const {
    const String area = stringProperty(commandValue, "area", "window");
    const File path = resolveCommandPath(stringProperty(commandValue, "path"));
    Component* component = componentForArea(area);

    if (component == nullptr) {
        return failedResult("screenshot", "Unknown area: " + area);
    }
    if (path == File()) {
        return failedResult("screenshot", "Missing screenshot path");
    }
    if (component->getWidth() <= 0 || component->getHeight() <= 0) {
        return failedResult("screenshot", "Cannot capture zero-size area: " + area);
    }

    Image image = component->createComponentSnapshot(component->getLocalBounds());
    PNGImageFormat format;
    path.getParentDirectory().createDirectory();
    path.deleteFile();
    std::unique_ptr<FileOutputStream> stream(path.createOutputStream());

    if (stream == nullptr || !format.writeImageToStream(image, *stream)) {
        return failedResult("screenshot", "Could not write screenshot: " + path.getFullPathName());
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("area", area);
    object->setProperty("path", path.getFullPathName());
    object->setProperty("width", image.getWidth());
    object->setProperty("height", image.getHeight());
    object->setProperty("bounds", rectangleToVar(component->getBounds()));
    object->setProperty("screenBounds", rectangleToVar(component->getScreenBounds()));
    return okResult("screenshot", data);
}

var CycleV2Automation::waitForIdle(const var& commandValue) const {
    const int delayMs = intProperty(commandValue, "delayMs", intProperty(commandValue, "idleDelayMs", 0));

    if (delayMs > 0) {
        if (MessageManager::getInstance()->isThisTheMessageThread()) {
            MessageManager::getInstance()->runDispatchLoopUntil(delayMs);
        } else {
            Thread::sleep((uint32) delayMs);
        }
    }

    return okResult("waitForIdle");
}

Component* CycleV2Automation::componentForArea(const String& area) const {
    if (area == "window" || area == "AreaWindow") {
        return const_cast<Component*>(&window);
    }
    if (area == "workspace" || area == "AreaWorkspace") {
        return const_cast<NodeWorkspace*>(&workspace);
    }
    if (area == "canvas" || area == "AreaNodeCanvas") {
        return const_cast<NodeCanvas*>(&workspace.getCanvas());
    }
    if (area == "performanceKeyboard" || area == "AreaPerformanceKeyboard") {
        return const_cast<PerformanceKeyboardPanel*>(
                &workspace.performanceKeyboardForAutomation());
    }

    return nullptr;
}

var CycleV2Automation::componentInfo(const String& area, Component& component) const {
    var target = makeObject();
    auto* object = objectFor(target);
    object->setProperty("area", area);
    object->setProperty("target", "");
    object->setProperty("resolved", true);
    object->setProperty("visible", component.isVisible());
    object->setProperty("showing", component.isShowing());
    object->setProperty("enabled", component.isEnabled());
    object->setProperty("bounds", rectangleToVar(component.getBounds()));
    object->setProperty("screenBounds", rectangleToVar(component.getScreenBounds()));
    return target;
}

void CycleV2Automation::writeReport(const var& report) const {
    const File reportFile = options.reportFile == File()
            ? File::getSpecialLocation(File::tempDirectory).getChildFile("cycle-v2-agent-report.json")
            : options.reportFile;

    reportFile.getParentDirectory().createDirectory();
    std::unique_ptr<FileOutputStream> stream(reportFile.createOutputStream());

    if (stream != nullptr) {
        stream->writeText(JSON::toString(report, true) + "\n", false, false, nullptr);
    }
}

}
