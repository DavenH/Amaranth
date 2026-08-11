#include "PanelInputHostComponent.h"

#include <Inter/Interactor.h>
#include <UI/Panels/Panel.h>

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
    if (Interactor* interactor = panelInteractor()) {
        interactor->mouseWheelMove(event, wheel);
    }
}

bool PanelInputHostComponent::keyPressed(const KeyPress& key) {
    if (key != KeyPress::deleteKey && key != KeyPress::backspaceKey) {
        return false;
    }
    return deleteKeyPressed();
}

void PanelInputHostComponent::resized() {
    panel.panelResized();
}
