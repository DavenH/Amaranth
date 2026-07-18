#include "CycleV2Automation.h"

#include "../UI/NodeWorkspace.h"

#include <cerrno>
#include <cmath>
#include <cstring>

#if JUCE_MAC || JUCE_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace CycleV2 {

namespace {

var makeObject() {
    return new DynamicObject();
}

DynamicObject* objectFor(var& value) {
    return value.getDynamicObject();
}

const DynamicObject* objectFor(const var& value) {
    return value.getDynamicObject();
}

bool compareValues(const var& actual, const String& op, const var& expected);

String stringProperty(const var& value, const Identifier& property, const String& fallback = {}) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : found.toString();
    }

    return fallback;
}

bool boolProperty(const var& value, const Identifier& property, bool fallback = false) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (bool) found;
    }

    return fallback;
}

int intProperty(const var& value, const Identifier& property, int fallback = 0) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (int) found;
    }

    return fallback;
}

float floatProperty(const var& value, const Identifier& property, float fallback = 0.f) {
    if (const auto* object = objectFor(value)) {
        const var found = object->getProperty(property);
        return found.isVoid() ? fallback : (float) (double) found;
    }

    return fallback;
}

var okResult(const String& type, var data = {}) {
    var result = makeObject();
    auto* object = objectFor(result);
    object->setProperty("ok", true);
    object->setProperty("type", type);

    if (!data.isVoid()) {
        object->setProperty("data", data);
    }

    return result;
}

var failedResult(const String& type, const String& message) {
    var result = makeObject();
    auto* object = objectFor(result);
    object->setProperty("ok", false);
    object->setProperty("type", type);
    object->setProperty("message", message);
    return result;
}

var rectangleToVar(Rectangle<int> bounds) {
    var result = makeObject();
    auto* object = objectFor(result);
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return result;
}

Rectangle<float> rectangleFromVar(const var& value) {
    const auto* object = objectFor(value);

    if (object == nullptr) {
        return {};
    }

    return {
            (float) (double) object->getProperty("x"),
            (float) (double) object->getProperty("y"),
            (float) (double) object->getProperty("width"),
            (float) (double) object->getProperty("height")
    };
}

bool pointerTargetBounds(const var& targetsValue, const String& targetId, Rectangle<float>& bounds) {
    const auto* object = objectFor(targetsValue);

    if (object == nullptr) {
        return false;
    }

    const var targetArrayValue = object->getProperty("targets");
    const Array<var>* targets = targetArrayValue.getArray();

    if (targets == nullptr) {
        return false;
    }

    for (const auto& target : *targets) {
        const auto* targetObject = objectFor(target);

        if (targetObject == nullptr || targetObject->getProperty("id").toString() != targetId) {
            continue;
        }

        bounds = rectangleFromVar(targetObject->getProperty("bounds"));
        return !bounds.isEmpty();
    }

    return false;
}

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

bool getPathValue(const var& root, const String& path, var& result) {
    if (path.isEmpty()) {
        result = root;
        return true;
    }

    var current = root;
    StringArray parts;
    parts.addTokens(path, ".", {});

    for (const auto& part : parts) {
        if (auto* array = current.getArray()) {
            const int index = part.getIntValue();

            if (index < 0 || index >= array->size()) {
                return false;
            }

            current = array->getReference(index);
            continue;
        }

        const auto* object = objectFor(current);
        if (object == nullptr) {
            return false;
        }

        current = object->getProperty(Identifier(part));
        if (current.isVoid()) {
            return false;
        }
    }

    result = current;
    return true;
}

void flattenPaths(const var& value, const String& prefix, Array<var>& paths) {
    if (const auto* object = objectFor(value)) {
        const NamedValueSet& properties = object->getProperties();

        for (int i = 0; i < properties.size(); ++i) {
            const auto propertyName = properties.getName(i).toString();
            const String next = prefix.isEmpty() ? propertyName : prefix + "." + propertyName;
            flattenPaths(properties.getValueAt(i), next, paths);
        }

        return;
    }

    if (const auto* array = value.getArray()) {
        for (int i = 0; i < array->size(); ++i) {
            const String next = prefix.isEmpty() ? String(i) : prefix + "." + String(i);
            flattenPaths(array->getReference(i), next, paths);
        }

        return;
    }

    var entry = makeObject();
    auto* object = objectFor(entry);
    object->setProperty("path", prefix);
    object->setProperty("value", value);
    object->setProperty("type", value.isBool() ? "bool" : value.isDouble() || value.isInt() ? "number" : "string");
    paths.add(entry);
}

bool valuesMatch(const var& actual, const var& expected) {
    if (actual.isDouble() || actual.isInt() || expected.isDouble() || expected.isInt()) {
        return std::abs((double) actual - (double) expected) < 0.000001;
    }

    return actual == expected;
}

bool compareValues(const var& actual, const String& op, const var& expected) {
    if (op == "exists") {
        return true;
    }
    if (op == "equals") {
        return valuesMatch(actual, expected);
    }
    if (op == "notEquals") {
        return !valuesMatch(actual, expected);
    }

    const double actualNumber = (double) actual;
    const double expectedNumber = (double) expected;

    if (op == "lessThan") {
        return actualNumber < expectedNumber;
    }
    if (op == "lessThanOrEqual") {
        return actualNumber <= expectedNumber;
    }
    if (op == "greaterThan") {
        return actualNumber > expectedNumber;
    }
    if (op == "greaterThanOrEqual") {
        return actualNumber >= expectedNumber;
    }

    return false;
}

Point<float> getPointerPosition(const var& command, Component& component, const String& xName, const String& yName) {
    const Rectangle<int> bounds = component.getLocalBounds();
    const auto* object = objectFor(command);
    const var xValue = object == nullptr ? var() : object->getProperty(Identifier(xName));
    const var yValue = object == nullptr ? var() : object->getProperty(Identifier(yName));

    if (!xValue.isVoid() && !yValue.isVoid()) {
        return { (float) (double) xValue, (float) (double) yValue };
    }

    const String normalizedXName = xName == "downX" ? "normalizedDownX" : "normalizedX";
    const String normalizedYName = yName == "downY" ? "normalizedDownY" : "normalizedY";
    var normalizedX = object == nullptr ? var() : object->getProperty(Identifier(normalizedXName));
    var normalizedY = object == nullptr ? var() : object->getProperty(Identifier(normalizedYName));

    if ((normalizedX.isVoid() || normalizedY.isVoid()) && xName != "x" && yName != "y") {
        normalizedX = object == nullptr ? var() : object->getProperty("normalizedX");
        normalizedY = object == nullptr ? var() : object->getProperty("normalizedY");
    }

    if (!normalizedX.isVoid() && !normalizedY.isVoid()) {
        return {
                bounds.getWidth() * (float) (double) normalizedX,
                bounds.getHeight() * (float) (double) normalizedY
        };
    }

    return bounds.getCentre().toFloat();
}

ModifierKeys pointerModifiers(const var& command, bool buttonDown) {
    ModifierKeys modifiers = ModifierKeys::currentModifiers.withoutMouseButtons();

    if (buttonDown) {
        const String button = stringProperty(command, "button", stringProperty(command, "mouseButton")).toLowerCase();

        if (boolProperty(command, "right") || button == "right" || button == "secondary") {
            modifiers = modifiers.withFlags(ModifierKeys::rightButtonModifier);
        } else if (boolProperty(command, "middle") || button == "middle") {
            modifiers = modifiers.withFlags(ModifierKeys::middleButtonModifier);
        } else {
            modifiers = modifiers.withFlags(ModifierKeys::leftButtonModifier);
        }
    }

    if (boolProperty(command, "shift")) {
        modifiers = modifiers.withFlags(ModifierKeys::shiftModifier);
    }
    if (boolProperty(command, "command")) {
        modifiers = modifiers.withFlags(ModifierKeys::commandModifier);
    }
    if (boolProperty(command, "ctrl")) {
        modifiers = modifiers.withFlags(ModifierKeys::ctrlModifier);
    }
    if (boolProperty(command, "alt")) {
        modifiers = modifiers.withFlags(ModifierKeys::altModifier);
    }

    return modifiers;
}

MouseEvent makePointerEvent(
        Component& component,
        Point<float> position,
        Point<float> downPosition,
        const var& command,
        bool buttonDown,
        bool wasDragged,
        int clickCount) {
    const Time now = Time::getCurrentTime();

    return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            pointerModifiers(command, buttonDown),
            MouseInputSource::defaultPressure,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            &component,
            &component,
            now,
            downPosition,
            now,
            clickCount,
            wasDragged
    };
}

}

class CycleV2Automation::SessionServer :
        public Thread {
public:
    SessionServer(CycleV2Automation& owner, String socketPath) :
            Thread("CycleV2AutomationSession")
        ,   owner(owner)
        ,   socketPath(std::move(socketPath)) {
    }

    ~SessionServer() override {
        signalThreadShouldExit();
        closeServerSocket();
        stopThread(1000);
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());
      #else
        File(socketPath).deleteFile();
      #endif
    }

    bool start(String& message) {
      #if JUCE_MAC || JUCE_LINUX
        ::unlink(socketPath.toRawUTF8());

        serverFd = ::socket(AF_UNIX, SOCK_STREAM, 0);

        if (serverFd < 0) {
            message = "Could not create Cycle V2 session socket: " + String(std::strerror(errno));
            return false;
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const auto path = socketPath.toRawUTF8();
        std::strncpy(address.sun_path, path, sizeof(address.sun_path) - 1);

        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            message = "Could not bind Cycle V2 session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }

        if (::listen(serverFd, 8) != 0) {
            message = "Could not listen on Cycle V2 session socket: " + String(std::strerror(errno));
            closeServerSocket();
            return false;
        }

        startThread();
        message = "Cycle V2 automation session listening: " + socketPath;
        return true;
      #else
        ignoreUnused(message);
        return false;
      #endif
    }

    void run() override {
      #if JUCE_MAC || JUCE_LINUX
        while (!threadShouldExit()) {
            int clientFd = ::accept(serverFd, nullptr, nullptr);

            if (clientFd < 0) {
                if (!threadShouldExit()) {
                    Thread::sleep(25);
                }

                continue;
            }

            handleClient(clientFd);
            ::close(clientFd);
        }
      #endif
    }

private:
    void closeServerSocket() {
      #if JUCE_MAC || JUCE_LINUX
        if (serverFd >= 0) {
            ::shutdown(serverFd, SHUT_RDWR);
            ::close(serverFd);
            serverFd = -1;
        }
      #endif
    }

    static var errorResponse(const String& message) {
        var response = makeObject();
        auto* object = objectFor(response);
        object->setProperty("ok", false);
        object->setProperty("message", message);
        return response;
    }

    void handleClient(int clientFd) {
      #if JUCE_MAC || JUCE_LINUX
        String requestText;
        char buffer[1024];

        while (!threadShouldExit()) {
            const ssize_t count = ::read(clientFd, buffer, sizeof(buffer));

            if (count <= 0) {
                break;
            }

            requestText += String::fromUTF8(buffer, int(count));

            if (requestText.containsChar('\n')) {
                requestText = requestText.upToFirstOccurrenceOf("\n", false, false);
                break;
            }
        }

        const var request = JSON::parse(requestText);
        auto completed = std::make_shared<WaitableEvent>();
        auto response = std::make_shared<var>();

        const bool dispatched = MessageManager::callAsync([this, request, response, completed] {
            *response = owner.handleSessionRequest(request);
            completed->signal();
        });

        if (dispatched) {
            if (!completed->wait(30000)) {
                *response = errorResponse("Timed out waiting for Cycle V2 message thread");
            }
        } else {
            *response = errorResponse("Could not dispatch Cycle V2 session request to message thread");
        }

        const String responseText = JSON::toString(*response, true) + "\n";
        const CharPointer_UTF8 utf8 = responseText.toUTF8();
        ::write(clientFd, utf8.getAddress(), std::strlen(utf8.getAddress()));
      #else
        ignoreUnused(clientFd);
      #endif
    }

    CycleV2Automation& owner;
    String socketPath;
    int serverFd { -1 };
};

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
}

CycleV2Automation::~CycleV2Automation() {
    sessionServer = nullptr;
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
    if (sessionServer != nullptr || !options.hasSession) {
        return;
    }

    if (options.sessionPath.isEmpty()) {
        DBG("Cycle V2 automation session path is empty");
        return;
    }

    String message;
    sessionServer = std::make_unique<SessionServer>(*this, options.sessionPath);

    if (!sessionServer->start(message)) {
        DBG(message);
        sessionServer = nullptr;
        return;
    }

    DBG(message);
}

var CycleV2Automation::runCommand(const var& commandValue) {
    const String command = stringProperty(commandValue, "command");

    if (command == "snapshotState") {
        return okResult(command, snapshotState());
    }
    if (command == "inspectTargets") {
        return inspectTargets(commandValue);
    }
    if (command == "inspectPointerTargets") {
        return inspectPointerTargets();
    }
    if (command == "inspectOpenGLDiagnostics") {
        return inspectOpenGLDiagnostics();
    }
    if (command == "exportGraph") {
        return exportGraph(commandValue);
    }
    if (command == "openGraph") {
        return openGraph(commandValue);
    }
    if (command == "saveGraph") {
        return saveGraph(commandValue);
    }
    if (command == "listMenuItems" || command == "listMenus") {
        return listMenuItems();
    }
    if (command == "invokeMenuItem") {
        return invokeMenuItem(commandValue);
    }
    if (command == "listPaletteItems") {
        return listPaletteItems();
    }
    if (command == "invokePaletteItem") {
        return invokePaletteItem(commandValue);
    }
    if (command == "captureAudio") {
        return captureAudio(commandValue);
    }
    if (command == "openNodeEditor" || command == "openMeshPopup") {
        return openNodeEditor(commandValue);
    }
    if (command == "addNode") {
        return addNode(commandValue);
    }
    if (command == "moveNode") {
        return moveNode(commandValue);
    }
    if (command == "connectPorts" || command == "connect") {
        return connectPorts(commandValue);
    }
    if (command == "deleteNode" || command == "removeNode") {
        return deleteNode(commandValue);
    }
    if (command == "deleteEdge" || command == "removeEdge") {
        return deleteEdge(commandValue);
    }
    if (command == "setNodeParameter") {
        return setNodeParameter(commandValue);
    }
    if (command == "inspectNodeControls") {
        return inspectNodeControls(commandValue);
    }
    if (command == "setMorphSlider") {
        return setMorphSlider(commandValue);
    }
    if (command == "setPrimaryAxis") {
        return setPrimaryAxis(commandValue);
    }
    if (command == "toggleLink") {
        return toggleLink(commandValue);
    }
    if (command == "selectVertex") {
        return selectVertex(commandValue);
    }
    if (command == "setVertexParameter") {
        return setVertexParameter(commandValue);
    }
    if (command == "pointer") {
        return pointer(commandValue);
    }
    if (command == "screenshot") {
        return screenshot(commandValue);
    }
    if (command == "assertState") {
        return assertState(commandValue);
    }
    if (command == "assertNodeParameter") {
        return assertNodeParameter(commandValue);
    }
    if (command == "listAssertionPaths") {
        return listAssertionPaths();
    }
    if (command == "waitForIdle") {
        return waitForIdle(commandValue);
    }
    if (command == "quit") {
        return okResult(command);
    }

    return failedResult(command.isEmpty() ? "unknown" : command, "Unknown Cycle V2 automation command");
}

var CycleV2Automation::handleSessionRequest(const var& request) {
    var response = makeObject();
    auto* responseObject = objectFor(response);
    responseObject->setProperty("ok", false);

    if (request.isVoid()) {
        responseObject->setProperty("message", "Invalid JSON request");
        return response;
    }

    if (const auto* requestObject = objectFor(request)) {
        const var id = requestObject->getProperty("id");

        if (!id.isVoid()) {
            responseObject->setProperty("id", id);
        }

        var command = requestObject->getProperty("command");

        if (command.isVoid()) {
            command = request;
        }

        const var result = runCommand(command);
        const bool ok = boolProperty(result, "ok");
        responseObject->setProperty("ok", ok);
        responseObject->setProperty("result", result);

        if (stringProperty(command, "command") == "quit") {
            MessageManager::callAsync([] {
                JUCEApplicationBase::quit();
            });
        }

        return response;
    }

    const var result = runCommand(request);
    responseObject->setProperty("ok", boolProperty(result, "ok"));
    responseObject->setProperty("result", result);
    return response;
}

var CycleV2Automation::snapshotState() const {
    var state = workspace.exportAutomationState();

    if (auto* object = objectFor(state)) {
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
    const File path(stringProperty(commandValue, "path"));
    const String xml = workspace.exportGraphXml();

    if (path == File()) {
        var data = makeObject();
        objectFor(data)->setProperty("xml", xml);
        return okResult("exportGraph", data);
    }

    path.getParentDirectory().createDirectory();
    if (!path.replaceWithText(xml)) {
        return failedResult("exportGraph", "Could not write graph: " + path.getFullPathName());
    }

    var data = makeObject();
    objectFor(data)->setProperty("path", path.getFullPathName());
    return okResult("exportGraph", data);
}

var CycleV2Automation::openGraph(const var& commandValue) {
    const File path(stringProperty(commandValue, "path"));

    if (!workspace.loadGraphFromFile(path)) {
        return failedResult("openGraph", "Could not open graph: " + path.getFullPathName());
    }

    return okResult("openGraph", snapshotState());
}

var CycleV2Automation::saveGraph(const var& commandValue) {
    const File path(stringProperty(commandValue, "path"));

    if (!workspace.saveGraphToFile(path)) {
        return failedResult("saveGraph", "Could not save graph: " + path.getFullPathName());
    }

    var data = makeObject();
    objectFor(data)->setProperty("path", path.getFullPathName());
    return okResult("saveGraph", data);
}

var CycleV2Automation::listMenuItems() const {
    Array<var> items;
    items.add(menuItemToVar("file.openGraph", "File", "Open Graph...", true));
    items.add(menuItemToVar("file.saveGraph", "File", "Save Graph", true));
    items.add(menuItemToVar("file.saveGraphAs", "File", "Save Graph As...", true));

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
    items.add(paletteItemToVar("guideCurve", "Control", "Guide"));
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
    return addNode(addCommand);
}

var CycleV2Automation::captureAudio(const var& commandValue) {
    const int frameCount = jlimit(1, 262144, intProperty(commandValue, "frames", intProperty(commandValue, "samples", 4096)));
    const File path(stringProperty(commandValue, "path"));
    var data = workspace.captureAudioForAutomation((size_t) frameCount);
    const auto* dataObject = objectFor(data);

    if (dataObject == nullptr || !(bool) dataObject->getProperty("compileSucceeded")) {
        return failedResult("captureAudio", "Cannot capture audio from an uncompiled graph");
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

var CycleV2Automation::openNodeEditor(const var& commandValue) {
    const String command = stringProperty(commandValue, "command", "openNodeEditor");
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult(command, "Missing nodeId");
    }

    if (!workspace.openNodeEditorForAutomation(nodeId)) {
        return failedResult(command, "Could not open editor for node: " + nodeId);
    }

    var data = workspace.inspectNodeControlsForAutomation(nodeId);
    return okResult(command, data);
}

var CycleV2Automation::inspectPointerTargets() const {
    return okResult("inspectPointerTargets", workspace.inspectPointerTargetsForAutomation());
}

var CycleV2Automation::inspectOpenGLDiagnostics() const {
    return okResult("inspectOpenGLDiagnostics", workspace.inspectOpenGLDiagnosticsForAutomation());
}

var CycleV2Automation::addNode(const var& commandValue) {
    const String kind = stringProperty(commandValue, "kind");
    const Point<float> position {
            floatProperty(commandValue, "x", 0.f),
            floatProperty(commandValue, "y", 0.f)
    };
    String nodeId;

    if (kind.isEmpty()) {
        return failedResult("addNode", "Missing kind");
    }

    if (!workspace.addNodeForAutomation(kind, position, nodeId)) {
        return failedResult("addNode", "Could not add node kind: " + kind);
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("nodeId", nodeId);
    object->setProperty("kind", kind);
    return okResult("addNode", data);
}

var CycleV2Automation::moveNode(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const Point<float> position {
            floatProperty(commandValue, "x", 0.f),
            floatProperty(commandValue, "y", 0.f)
    };

    if (nodeId.isEmpty()) {
        return failedResult("moveNode", "Missing nodeId");
    }

    if (!workspace.moveNodeForAutomation(nodeId, position)) {
        return failedResult("moveNode", "Could not move node: " + nodeId);
    }

    return okResult("moveNode", snapshotState());
}

var CycleV2Automation::connectPorts(const var& commandValue) {
    const String sourceNodeId = stringProperty(commandValue, "sourceNodeId");
    const String sourcePortId = stringProperty(commandValue, "sourcePortId");
    const String destNodeId = stringProperty(commandValue, "destNodeId");
    const String destPortId = stringProperty(commandValue, "destPortId");

    if (sourceNodeId.isEmpty() || sourcePortId.isEmpty() || destNodeId.isEmpty() || destPortId.isEmpty()) {
        return failedResult("connectPorts", "Missing source or destination port address");
    }

    if (!workspace.connectPortsForAutomation(sourceNodeId, sourcePortId, destNodeId, destPortId)) {
        return failedResult("connectPorts", "Could not connect "
                + sourceNodeId + "." + sourcePortId + " -> " + destNodeId + "." + destPortId);
    }

    return okResult("connectPorts", snapshotState());
}

var CycleV2Automation::deleteNode(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult("deleteNode", "Missing nodeId");
    }

    if (!workspace.deleteNodeForAutomation(nodeId)) {
        return failedResult("deleteNode", "Could not delete node: " + nodeId);
    }

    return okResult("deleteNode", snapshotState());
}

var CycleV2Automation::deleteEdge(const var& commandValue) {
    const int edgeIndex = intProperty(commandValue, "edgeIndex", intProperty(commandValue, "index", -1));

    if (!workspace.deleteEdgeForAutomation(edgeIndex)) {
        return failedResult("deleteEdge", "Could not delete edge index: " + String(edgeIndex));
    }

    return okResult("deleteEdge", snapshotState());
}

var CycleV2Automation::setNodeParameter(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const String label = stringProperty(commandValue, "label", parameterId);
    const String value = stringProperty(commandValue, "value");

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("setNodeParameter", "Missing nodeId or parameterId");
    }

    if (!workspace.setNodeParameterForAutomation(nodeId, parameterId, label, value)) {
        return failedResult("setNodeParameter", "Could not set parameter: " + nodeId + "." + parameterId);
    }

    return okResult("setNodeParameter", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::inspectNodeControls(const var& commandValue) const {
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult("inspectNodeControls", "Missing nodeId");
    }

    var data = workspace.inspectNodeControlsForAutomation(nodeId);
    if (const auto* object = objectFor(data); object == nullptr || !(bool) object->getProperty("resolved")) {
        return failedResult("inspectNodeControls", "Unknown node: " + nodeId);
    }

    return okResult("inspectNodeControls", data);
}

var CycleV2Automation::setMorphSlider(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis", stringProperty(commandValue, "parameterId"));
    const float value = (float) (objectFor(commandValue) == nullptr ? 0.0 : (double) objectFor(commandValue)->getProperty("value"));

    if (nodeId.isEmpty()) {
        return failedResult("setMorphSlider", "Missing nodeId");
    }
    if (axis.isEmpty()) {
        return failedResult("setMorphSlider", "Missing axis");
    }

    if (!workspace.setMorphSliderForAutomation(nodeId, axis, value)) {
        return failedResult("setMorphSlider", "Could not set morph slider " + nodeId + "." + axis);
    }

    return okResult("setMorphSlider", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::setPrimaryAxis(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis");

    if (nodeId.isEmpty() || axis.isEmpty()) {
        return failedResult("setPrimaryAxis", "Missing nodeId or axis");
    }

    if (!workspace.setPrimaryAxisForAutomation(nodeId, axis)) {
        return failedResult("setPrimaryAxis", "Could not set primary axis: " + nodeId + "." + axis);
    }

    return okResult("setPrimaryAxis", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::toggleLink(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis");

    if (nodeId.isEmpty() || axis.isEmpty()) {
        return failedResult("toggleLink", "Missing nodeId or axis");
    }

    if (!workspace.toggleLinkForAutomation(nodeId, axis)) {
        return failedResult("toggleLink", "Could not toggle link: " + nodeId + "." + axis);
    }

    return okResult("toggleLink", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::selectVertex(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const int vertexIndex = intProperty(commandValue, "vertexIndex", intProperty(commandValue, "index", -1));

    if (nodeId.isEmpty() || vertexIndex < 0) {
        return failedResult("selectVertex", "Missing nodeId or vertexIndex");
    }

    if (!workspace.selectVertexForAutomation(nodeId, vertexIndex)) {
        return failedResult("selectVertex", "Could not select vertex: " + nodeId + "#" + String(vertexIndex));
    }

    return okResult("selectVertex", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::setVertexParameter(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const float value = floatProperty(commandValue, "value", 0.f);

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("setVertexParameter", "Missing nodeId or parameterId");
    }

    if (!workspace.setVertexParameterForAutomation(nodeId, parameterId, value)) {
        return failedResult("setVertexParameter", "Could not set vertex parameter: " + nodeId + "." + parameterId);
    }

    return okResult("setVertexParameter", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2Automation::pointer(const var& commandValue) {
    String area = stringProperty(commandValue, "area", "canvas");
    const String targetId = stringProperty(commandValue, "targetId");
    const String downTargetId = stringProperty(commandValue, "downTargetId");

    if (targetId.isNotEmpty() || downTargetId.isNotEmpty()) {
        area = "canvas";
    }

    Component* component = componentForArea(area);

    if (component == nullptr) {
        return failedResult("pointer", "Pointer target could not be resolved: " + area);
    }
    if (!component->isShowing()) {
        return failedResult("pointer", "Pointer target is not showing: " + area);
    }
    if (component->getLocalBounds().isEmpty()) {
        return failedResult("pointer", "Pointer target has empty bounds: " + area);
    }

    const String eventType = stringProperty(commandValue, "event", stringProperty(commandValue, "pointerEvent", "click"));
    auto resolveTargetPosition = [&](const String& id, bool down, bool& resolved) -> Point<float> {
        resolved = true;

        if (id.isEmpty()) {
            return getPointerPosition(commandValue, *component, down ? "downX" : "x", down ? "downY" : "y");
        }

        Rectangle<float> targetBounds;
        const var targets = workspace.inspectPointerTargetsForAutomation();

        if (!pointerTargetBounds(targets, id, targetBounds)) {
            resolved = false;
            return getPointerPosition(commandValue, *component, down ? "downX" : "x", down ? "downY" : "y");
        }

        const float normalizedX = floatProperty(commandValue, down ? "downTargetX" : "targetX", 0.5f);
        const float normalizedY = floatProperty(commandValue, down ? "downTargetY" : "targetY", 0.5f);
        return {
                targetBounds.getX() + targetBounds.getWidth() * normalizedX,
                targetBounds.getY() + targetBounds.getHeight() * normalizedY
        };
    };
    bool targetResolved {};
    bool downTargetResolved {};
    Point<float> position = resolveTargetPosition(targetId, false, targetResolved);
    Point<float> downPosition = resolveTargetPosition(downTargetId, true, downTargetResolved);

    if (!targetResolved) {
        return failedResult("pointer", "Pointer target id could not be resolved: " + targetId);
    }
    if (!downTargetResolved) {
        return failedResult("pointer", "Pointer down target id could not be resolved: " + downTargetId);
    }

    if (targetId.startsWith("expanded:") && eventType == "click") {
        const String suffix = targetId.fromFirstOccurrenceOf("expanded:", false, false);
        const String nodeId = suffix.upToFirstOccurrenceOf(".", false, false);
        const String target = suffix.fromFirstOccurrenceOf(".", false, false);
        const String targetKind = target.upToFirstOccurrenceOf(".", false, false);
        const String targetValue = target.fromFirstOccurrenceOf(".", false, false);

        var semanticCommand = makeObject();
        auto* semanticObject = objectFor(semanticCommand);
        semanticObject->setProperty("nodeId", nodeId);

        if (targetKind == "trimeshPrimaryAxis") {
            semanticObject->setProperty("axis", targetValue);
            var result = setPrimaryAxis(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshLinkToggle") {
            semanticObject->setProperty("axis", targetValue);
            var result = toggleLink(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshMorphRail") {
            semanticObject->setProperty("axis", targetValue);
            semanticObject->setProperty("value", floatProperty(commandValue, "targetX", 0.5f));
            var result = setMorphSlider(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshVertexParameter") {
            semanticObject->setProperty("parameterId", targetValue);
            semanticObject->setProperty("value", floatProperty(commandValue, "targetX", 0.5f));
            var result = setVertexParameter(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
    }

    Component* eventComponent = component;
    if (targetId.isNotEmpty()) {
        if (Component* hitComponent = component->getComponentAt(position.roundToInt())) {
            eventComponent = hitComponent;
            position = eventComponent->getLocalPoint(component, position);
            downPosition = eventComponent->getLocalPoint(component, downPosition);
        }
    }

    if (eventType == "click") {
        eventComponent->mouseDown(makePointerEvent(*eventComponent, position, position, commandValue, true, false, 1));
        eventComponent->mouseUp(makePointerEvent(*eventComponent, position, position, commandValue, false, false, 1));
    } else if (eventType == "doubleClick") {
        const MouseEvent doubleClick = makePointerEvent(*eventComponent, position, position, commandValue, true, false, 2);
        eventComponent->mouseDown(doubleClick);
        eventComponent->mouseDoubleClick(doubleClick);
        eventComponent->mouseUp(makePointerEvent(*eventComponent, position, position, commandValue, false, false, 2));
    } else if (eventType == "down") {
        eventComponent->mouseDown(makePointerEvent(*eventComponent, position, position, commandValue, true, false, 1));
    } else if (eventType == "up") {
        eventComponent->mouseUp(makePointerEvent(*eventComponent, position, downPosition, commandValue, false, false, 1));
    } else if (eventType == "drag") {
        eventComponent->mouseDrag(makePointerEvent(*eventComponent, position, downPosition, commandValue, true, true, 1));
    } else if (eventType == "move") {
        eventComponent->mouseMove(makePointerEvent(*eventComponent, position, position, commandValue, false, false, 0));
    } else if (eventType == "wheel") {
        MouseWheelDetails wheel {
                floatProperty(commandValue, "deltaX"),
                floatProperty(commandValue, "deltaY"),
                boolProperty(commandValue, "reversed"),
                boolProperty(commandValue, "smooth", true),
                boolProperty(commandValue, "inertial")
        };

        eventComponent->mouseWheelMove(makePointerEvent(*eventComponent, position, position, commandValue, false, false, 0), wheel);
    } else {
        return failedResult("pointer", "Unknown pointer event: " + eventType);
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("event", eventType);
    object->setProperty("area", area);
    object->setProperty("targetId", targetId);
    object->setProperty("targetComponent", eventComponent == component ? "area" : eventComponent->getName());
    object->setProperty("x", position.x);
    object->setProperty("y", position.y);
    object->setProperty("localBounds", rectangleToVar(component->getLocalBounds()));
    object->setProperty("screenBounds", rectangleToVar(component->getScreenBounds()));
    return okResult("pointer", data);
}

var CycleV2Automation::screenshot(const var& commandValue) const {
    const String area = stringProperty(commandValue, "area", "window");
    const File path(stringProperty(commandValue, "path"));
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

var CycleV2Automation::assertState(const var& commandValue) const {
    const String path = stringProperty(commandValue, "path");
    const auto* commandObject = objectFor(commandValue);
    const var equalsValue = commandObject == nullptr ? var() : commandObject->getProperty("equals");
    const var value = commandObject == nullptr ? var() : commandObject->getProperty("value");
    const String op = stringProperty(commandValue, "op", equalsValue.isVoid() ? "exists" : "equals");
    const var expected = !equalsValue.isVoid() ? equalsValue : value;

    var actual;
    const bool found = getPathValue(snapshotState(), path, actual);

    if (!found) {
        return failedResult("assertState", "State path not found: " + path);
    }

    if (!compareValues(actual, op, expected)) {
        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("path", path);
        object->setProperty("op", op);
        object->setProperty("expected", expected);
        object->setProperty("actual", actual);
        var result = failedResult("assertState", "State assertion failed: " + path);
        objectFor(result)->setProperty("data", data);
        return result;
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("path", path);
    object->setProperty("op", op);
    object->setProperty("actual", actual);
    return okResult("assertState", data);
}

var CycleV2Automation::assertNodeParameter(const var& commandValue) const {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const auto* commandObject = objectFor(commandValue);
    const var equalsValue = commandObject == nullptr ? var() : commandObject->getProperty("equals");
    const var value = commandObject == nullptr ? var() : commandObject->getProperty("value");
    const String op = stringProperty(commandValue, "op", equalsValue.isVoid() ? "equals" : "equals");
    const var expected = !equalsValue.isVoid() ? equalsValue : value;
    String actualString;

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("assertNodeParameter", "Missing nodeId or parameterId");
    }

    if (!workspace.getNodeParameterForAutomation(nodeId, parameterId, actualString)) {
        return failedResult("assertNodeParameter", "Node parameter not found: " + nodeId + "." + parameterId);
    }

    const var actual = actualString;
    if (!compareValues(actual, op, expected)) {
        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("nodeId", nodeId);
        object->setProperty("parameterId", parameterId);
        object->setProperty("op", op);
        object->setProperty("expected", expected);
        object->setProperty("actual", actual);
        var result = failedResult("assertNodeParameter", "Node parameter assertion failed: " + nodeId + "." + parameterId);
        objectFor(result)->setProperty("data", data);
        return result;
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("nodeId", nodeId);
    object->setProperty("parameterId", parameterId);
    object->setProperty("op", op);
    object->setProperty("actual", actual);
    return okResult("assertNodeParameter", data);
}

var CycleV2Automation::listAssertionPaths() const {
    Array<var> paths;
    flattenPaths(snapshotState(), {}, paths);

    var data = makeObject();
    objectFor(data)->setProperty("paths", paths);
    return okResult("listAssertionPaths", data);
}

var CycleV2Automation::waitForIdle(const var& commandValue) const {
    const int delayMs = intProperty(commandValue, "delayMs", intProperty(commandValue, "idleDelayMs", 0));

    if (delayMs > 0) {
        Thread::sleep((uint32) delayMs);
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
