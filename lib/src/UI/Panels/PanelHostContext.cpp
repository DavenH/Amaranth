#include "PanelHostContext.h"

#include "Panel.h"

void PanelHostCallbacks::requestRepaint(Panel* panel, PanelDirtyState::Flag flag) const {
    if (repaintCallback != nullptr) {
        repaintCallback(panel, flag);
    }
}

void PanelHostCallbacks::setMouseCursor(Panel* panel, const juce::MouseCursor& cursor) const {
    if (cursorCallback != nullptr) {
        cursorCallback(panel, cursor);
    }
}

void PanelHostCallbacks::setCursorCallback(CursorCallback callback) {
    cursorCallback = std::move(callback);
}

void PanelHostCallbacks::setRepaintCallback(RepaintCallback callback) {
    repaintCallback = std::move(callback);
}

PanelRenderContext PanelHostContext::createRenderContext() const {
    PanelRenderContext context;

    context.bounds = bounds.getSmallestIntegerContainer();
    context.clip = clip.isEmpty() ? context.bounds : clip.getSmallestIntegerContainer();
    context.dirtyMask = dirtyMask;
    context.panelId = panelId;
    context.scaleFactor = scaleFactor;
    context.usesCachedSurface = usesCachedSurface;

    return context;
}

PanelPointerEvent PanelHostContext::createPointerEvent(
    juce::Point<float> hostPosition,
    juce::ModifierKeys modifiers,
    int clickCount
) const {
    PanelPointerEvent event;

    event.localPosition = hostToLocal(hostPosition, bounds);
    event.bounds = bounds;
    event.modifiers = modifiers;
    event.clickCount = clickCount;
    event.leftButtonDown = modifiers.isLeftButtonDown();
    event.middleButtonDown = modifiers.isMiddleButtonDown();
    event.rightButtonDown = modifiers.isRightButtonDown();

    return event;
}

juce::Point<float> PanelHostContext::hostToLocal(
    juce::Point<float> hostPosition,
    const juce::Rectangle<float>& panelBounds
) {
    return hostPosition - panelBounds.getPosition();
}

bool PanelHostContext::containsHostPoint(
    juce::Point<float> hostPosition,
    const juce::Rectangle<float>& panelBounds
) {
    return panelBounds.contains(hostPosition);
}
