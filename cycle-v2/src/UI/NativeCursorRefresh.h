#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

inline void showNativeCursorForPanel(
        juce::Component& panel,
        const juce::MouseCursor& cursor) {
    if (!panel.getScreenBounds().contains(juce::Desktop::getMousePosition())) {
        return;
    }

    juce::Component::SafePointer<juce::Component> safePanel(&panel);
    juce::MessageManager::callAsync([safePanel, cursor] {
        if (safePanel != nullptr
                && safePanel->getScreenBounds().contains(juce::Desktop::getMousePosition())) {
            juce::Desktop::getInstance().getMainMouseSource().showMouseCursor(cursor);
        }
    });
}

}
