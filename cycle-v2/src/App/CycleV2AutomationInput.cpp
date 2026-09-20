#include "App/CycleV2AutomationInput.h"

#include "App/CycleV2AutomationProtocol.h"
#include "UI/NodeWorkspace.h"

#include <utility>

namespace CycleV2 {

using namespace juce;
using namespace AutomationProtocol;

namespace {

bool automationKeyPress(const String& name, KeyPress& result) {
    if (name == "tab" || name == "shiftTab") {
        const ModifierKeys modifiers = name == "shiftTab"
                ? ModifierKeys::shiftModifier
                : ModifierKeys {};
        result = KeyPress(KeyPress::tabKey, modifiers, 0);
        return true;
    }

    const std::pair<const char*, int> keys[] {
            { "left", KeyPress::leftKey },
            { "right", KeyPress::rightKey },
            { "up", KeyPress::upKey },
            { "down", KeyPress::downKey },
            { "return", KeyPress::returnKey },
            { "space", KeyPress::spaceKey },
            { "escape", KeyPress::escapeKey },
            { "delete", KeyPress::deleteKey }
    };
    for (const auto& key : keys) {
        if (name == key.first) {
            result = KeyPress(key.second);
            return true;
        }
    }
    return false;
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

CycleV2AutomationInput::CycleV2AutomationInput(
        NodeWorkspace& targetWorkspace,
        ComponentResolver componentResolver,
        SemanticHandlers semanticHandlers) :
        workspace(targetWorkspace)
    ,   resolveComponent(std::move(componentResolver))
    ,   handlers(std::move(semanticHandlers)) {
}

var CycleV2AutomationInput::key(const var& commandValue) {
    const String area = stringProperty(commandValue, "area", "canvas");
    Component* component = resolveComponent(area);
    if (component == nullptr) {
        return failedResult("key", "Key area could not be resolved: " + area);
    }

    const String keyName = stringProperty(commandValue, "key");
    KeyPress keyPress;
    if (!automationKeyPress(keyName, keyPress)) {
        return failedResult("key", "Unknown key: " + keyName);
    }

    const bool handled = component->keyPressed(keyPress);
    var data = makeObject();
    objectFor(data)->setProperty("key", keyName);
    objectFor(data)->setProperty("handled", handled);
    return handled ? okResult("key", data) : failedResult("key", "Key was not handled: " + keyName);
}

var CycleV2AutomationInput::pointer(const var& commandValue) {
    String area = stringProperty(commandValue, "area", "canvas");
    const String targetId = stringProperty(commandValue, "targetId");
    const String downTargetId = stringProperty(commandValue, "downTargetId");

    if (targetId.startsWith("PerformanceKeyboard.")
            || downTargetId.startsWith("PerformanceKeyboard.")) {
        area = "workspace";
    } else if (targetId.isNotEmpty() || downTargetId.isNotEmpty()) {
        area = "canvas";
    }

    Component* component = resolveComponent(area);

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
    if (targetId == "PerformanceKeyboard.ModWheel") {
        const float position = jlimit(
                0.f,
                1.f,
                floatProperty(commandValue, "targetY", 0.5f));
        const int value = roundToInt((1.f - position) * 127.f);
        bool handled {};
        if (eventType == "down") {
            handled = workspace.performanceBeginModWheelGestureForAutomation(value);
        } else if (eventType == "drag") {
            handled = workspace.performanceUpdateModWheelGestureForAutomation(value);
        } else if (eventType == "up") {
            handled = workspace.performanceEndModWheelGestureForAutomation();
        } else {
            handled = workspace.performanceSetModWheelForAutomation(value);
        }
        if (!handled) {
            return failedResult("pointer", "Performance mod wheel gesture could not be applied");
        }

        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("event", eventType);
        object->setProperty("area", "workspace");
        object->setProperty("targetId", targetId);
        object->setProperty("value", value);
        return okResult("pointer", data);
    }
    if (targetId.startsWith("PerformanceKeyboard.Note")) {
        const int noteNumber = targetId.fromFirstOccurrenceOf(
                "PerformanceKeyboard.Note", false, false).getIntValue();
        const float velocity = jlimit(0.05f, 1.f, floatProperty(commandValue, "targetY", 0.8f));
        const String button = stringProperty(
                commandValue,
                "button",
                stringProperty(commandValue, "mouseButton")).toLowerCase();
        const bool selectsPreview = boolProperty(commandValue, "right")
                || button == "right"
                || button == "secondary";
        bool handled {};
        if (selectsPreview && (eventType == "down" || eventType == "click")) {
            handled = workspace.performanceSelectPreviewNoteForAutomation(noteNumber);
        } else if (eventType == "down") {
            handled = workspace.performancePointerDownForAutomation(noteNumber, velocity);
        } else if (eventType == "drag") {
            handled = workspace.performancePointerDragForAutomation(noteNumber, velocity);
        } else if (eventType == "up") {
            handled = workspace.performancePointerUpForAutomation();
        } else if (eventType == "click") {
            handled = workspace.performancePointerDownForAutomation(noteNumber, velocity)
                    && workspace.performancePointerUpForAutomation();
        }
        if (!handled) {
            return failedResult("pointer", "Performance keyboard gesture could not be applied");
        }

        var data = makeObject();
        auto* object = objectFor(data);
        object->setProperty("event", eventType);
        object->setProperty("area", "workspace");
        object->setProperty("targetId", targetId);
        object->setProperty("note", noteNumber);
        object->setProperty("velocity", velocity);
        return okResult("pointer", data);
    }
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
            var result = handlers.setPrimaryAxis(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshLinkToggle") {
            semanticObject->setProperty("axis", targetValue);
            var result = handlers.toggleLink(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshMorphRail") {
            semanticObject->setProperty("axis", targetValue);
            semanticObject->setProperty("value", floatProperty(commandValue, "targetX", 0.5f));
            var result = handlers.setMorphSlider(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
        if (targetKind == "trimeshVertexParameter") {
            semanticObject->setProperty("parameterId", targetValue);
            semanticObject->setProperty("value", floatProperty(commandValue, "targetX", 0.5f));
            var result = handlers.setVertexParameter(semanticCommand);
            objectFor(result)->setProperty("pointerTargetId", targetId);
            return result;
        }
    }

    Component* eventComponent = component;
    if (targetId.isNotEmpty() && !boolProperty(commandValue, "dispatchToArea")) {
        if (Component* hitComponent = component->getComponentAt(position.roundToInt())) {
            eventComponent = hitComponent;
            position = eventComponent->getLocalPoint(component, position);
            downPosition = eventComponent->getLocalPoint(component, downPosition);
        }
    }

    const String targetComponentName = eventComponent == component
            ? "area"
            : eventComponent->getName();
    Component::SafePointer<Component> safeEventComponent(eventComponent);
    String resolvedCursor = cursorName(eventComponent->getMouseCursor());

    if (eventType == "click") {
        safeEventComponent->mouseDown(makePointerEvent(
                *safeEventComponent, position, position, commandValue, true, false, 1));
        if (safeEventComponent != nullptr) {
            safeEventComponent->mouseUp(makePointerEvent(
                    *safeEventComponent, position, position, commandValue, false, false, 1));
        }
    } else if (eventType == "doubleClick") {
        const MouseEvent doubleClick = makePointerEvent(
                *safeEventComponent, position, position, commandValue, true, false, 2);
        safeEventComponent->mouseDown(doubleClick);
        if (safeEventComponent != nullptr) {
            safeEventComponent->mouseDoubleClick(doubleClick);
        }
        if (safeEventComponent != nullptr) {
            safeEventComponent->mouseUp(makePointerEvent(
                    *safeEventComponent, position, position, commandValue, false, false, 2));
        }
    } else if (eventType == "down") {
        safeEventComponent->mouseDown(makePointerEvent(
                *safeEventComponent, position, position, commandValue, true, false, 1));
    } else if (eventType == "up") {
        safeEventComponent->mouseUp(makePointerEvent(
                *safeEventComponent, position, downPosition, commandValue, false, false, 1));
    } else if (eventType == "drag") {
        safeEventComponent->mouseDrag(makePointerEvent(
                *safeEventComponent, position, downPosition, commandValue, true, true, 1));
    } else if (eventType == "move") {
        safeEventComponent->mouseMove(makePointerEvent(
                *safeEventComponent, position, position, commandValue, false, false, 0));
    } else if (eventType == "wheel") {
        MouseWheelDetails wheel {
                floatProperty(commandValue, "deltaX"),
                floatProperty(commandValue, "deltaY"),
                boolProperty(commandValue, "reversed"),
                boolProperty(commandValue, "smooth", true),
                boolProperty(commandValue, "inertial")
        };

        safeEventComponent->mouseWheelMove(
                makePointerEvent(*safeEventComponent, position, position, commandValue, false, false, 0),
                wheel);
    } else {
        return failedResult("pointer", "Unknown pointer event: " + eventType);
    }

    const bool targetDestroyed = safeEventComponent == nullptr;
    if (!targetDestroyed) {
        resolvedCursor = cursorName(safeEventComponent->getMouseCursor());
    }
    const String expectedCursor = stringProperty(commandValue, "expectedCursor");
    if (expectedCursor.isNotEmpty() && resolvedCursor != expectedCursor) {
        return failedResult(
                "pointer",
                "Expected cursor '" + expectedCursor + "' but resolved '" + resolvedCursor + "'");
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("event", eventType);
    object->setProperty("area", area);
    object->setProperty("targetId", targetId);
    object->setProperty("targetComponent", targetComponentName);
    object->setProperty("targetDestroyed", targetDestroyed);
    object->setProperty("cursor", resolvedCursor);
    object->setProperty("x", position.x);
    object->setProperty("y", position.y);
    object->setProperty("localBounds", rectangleToVar(component->getLocalBounds()));
    object->setProperty("screenBounds", rectangleToVar(component->getScreenBounds()));
    return okResult("pointer", data);
}

}
