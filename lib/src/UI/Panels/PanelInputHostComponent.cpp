#include "PanelInputHostComponent.h"

#include <Inter/Interactor.h>
#include <UI/Panels/Panel.h>

#include <cmath>

using namespace juce;

PanelInputHostComponent::PanelInputHostComponent(Panel& panelToHost) :
        panel(panelToHost) {
    setPaintingIsUnclipped(false);
    setInterceptsMouseClicks(true, true);
    setOpaque(false);
    setWantsKeyboardFocus(true);
}

Interactor* PanelInputHostComponent::panelInteractor() const {
    return panel.getInteractor().get();
}

bool PanelInputHostComponent::acceptsPointerDown(const MouseEvent& event) const {
    return event.mods.isLeftButtonDown() || event.mods.isRightButtonDown();
}

bool PanelInputHostComponent::acceptsDoubleClick(const MouseEvent&) const {
    return true;
}

void PanelInputHostComponent::mouseEnter(const MouseEvent& event) {
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseEnter(event);
        interactor->mouseMove(event);
    }
}

void PanelInputHostComponent::mouseMove(const MouseEvent& event) {
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseMove(event);
    }
}

void PanelInputHostComponent::mouseDown(const MouseEvent& event) {
    if (!acceptsPointerDown(event)) {
        pointerActive = false;
        return;
    }

    pointerActive = true;
    grabKeyboardFocus();
    pointerGestureBegan();
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseDown(event);
    }
    pointerGesturePressed();
}

void PanelInputHostComponent::mouseDoubleClick(const MouseEvent& event) {
    if (!acceptsDoubleClick(event)) {
        return;
    }

    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseDoubleClick(event);
    }
    pointerGestureUpdated();
}

void PanelInputHostComponent::mouseDrag(const MouseEvent& event) {
    if (!pointerActive) {
        return;
    }

    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseDrag(event);
    }
    pointerGestureUpdated();
}

void PanelInputHostComponent::mouseUp(const MouseEvent& event) {
    if (!pointerActive) {
        return;
    }

    pointerActive = false;
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseUp(event);
    }
    pointerGestureEnded();
}

void PanelInputHostComponent::mouseExit(const MouseEvent& event) {
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseExit(event);
    }
}

void PanelInputHostComponent::mouseWheelMove(
        const MouseEvent& event,
        const MouseWheelDetails& wheel) {
    if (pointerActive) {
        return;
    }

    const double nowMs = Time::getMillisecondCounterHiRes();
    if (!wheel.isSmooth || nowMs - lastWheelTimeMs > 180.0) {
        wheelAxis = WheelAxis::None;
        pendingWheelX = 0.f;
        pendingWheelY = 0.f;
    }
    lastWheelTimeMs = nowMs;

    if (wheel.isSmooth && wheelAxis == WheelAxis::None) {
        pendingWheelX += std::abs(wheel.deltaX);
        pendingWheelY += std::abs(wheel.deltaY);
        if (pendingWheelX + pendingWheelY < 0.025f) {
            return;
        }

        if (pendingWheelX > 0.f && pendingWheelX >= pendingWheelY * 0.75f) {
            wheelAxis = WheelAxis::Horizontal;
        } else if (pendingWheelY > 0.f) {
            wheelAxis = WheelAxis::Vertical;
        }
        pendingWheelX = 0.f;
        pendingWheelY = 0.f;
    }

    if (wheelAxis != WheelAxis::Vertical) {
        if (auto* zoomPanel = panel.getZoomPanel()) {
            zoomPanel->panHorizontal(wheel.deltaX);
        }
    }

    if (wheelAxis != WheelAxis::Horizontal && wheel.deltaY != 0.f) {
        if (Interactor* interactor = panelInteractor()) {
            interactor->mouseWheelMove(event, wheel);
        }
    }
}

bool PanelInputHostComponent::keyPressed(const KeyPress& key) {
    if (key == KeyPress::escapeKey && pointerActive && cancelPointerGesture()) {
        pointerActive = false;
        return true;
    }
    if (key != KeyPress::deleteKey && key != KeyPress::backspaceKey) {
        return false;
    }
    return deleteKeyPressed();
}

void PanelInputHostComponent::resized() {
    panel.panelResized();
}
