#pragma once

#include <cstdint>
#include <functional>

#include "JuceHeader.h"
#include "PanelDirtyState.h"
#include "PanelRenderContext.h"

class Panel;

class PanelHostCallbacks {
public:
    using RepaintCallback = std::function<void(Panel*, PanelDirtyState::Flag)>;
    using CursorCallback = std::function<void(Panel*, const juce::MouseCursor&)>;

    void requestRepaint(Panel* panel, PanelDirtyState::Flag flag) const;
    void setMouseCursor(Panel* panel, const juce::MouseCursor& cursor) const;
    void setCursorCallback(CursorCallback callback);
    void setRepaintCallback(RepaintCallback callback);

    bool hasCursorCallback() const { return cursorCallback != nullptr; }
    bool hasRepaintCallback() const { return repaintCallback != nullptr; }

private:
    CursorCallback cursorCallback;
    RepaintCallback repaintCallback;
};

class PanelPointerEvent {
public:
    juce::Point<float> localPosition;
    juce::Rectangle<float> bounds;
    juce::ModifierKeys modifiers;

    int clickCount = 0;
    bool leftButtonDown = false;
    bool middleButtonDown = false;
    bool rightButtonDown = false;
};

class PanelHostContext {
public:
    juce::Rectangle<float> bounds;
    juce::Rectangle<float> clip;

    uint32_t dirtyMask = 0;
    int panelId = -1;
    float scaleFactor = 1.f;
    bool usesCachedSurface = false;
    bool visible = true;

    PanelHostCallbacks callbacks;

    PanelRenderContext createRenderContext() const;
    PanelPointerEvent createPointerEvent(
        juce::Point<float> hostPosition,
        juce::ModifierKeys modifiers = {},
        int clickCount = 0
    ) const;

    static juce::Point<float> hostToLocal(
        juce::Point<float> hostPosition,
        const juce::Rectangle<float>& panelBounds
    );

    static bool containsHostPoint(
        juce::Point<float> hostPosition,
        const juce::Rectangle<float>& panelBounds
    );
};
