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
            auto& desktop = juce::Desktop::getInstance();
            auto* target = desktop.findComponentAt(juce::Desktop::getMousePosition());
            if (target != nullptr) {
                const auto resolved = target == safePanel.getComponent() || safePanel->isParentOf(target)
                        ? cursor : target->getLookAndFeel().getMouseCursorFor(*target);
                desktop.getMainMouseSource().showMouseCursor(resolved);
            }
        }
    });
}

}
