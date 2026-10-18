#pragma once

#include <JuceHeader.h>

#include <optional>

namespace CycleV2 {

inline juce::String morphAxisForKeyboardShortcut(const juce::KeyPress& key) {
    if (key.getModifiers().isAnyModifierKeyDown()) {
        return {};
    }

    switch (juce::CharacterFunctions::toLowerCase(key.getKeyCode())) {
        case 'y':   return "yellow";
        case 'r':   return "red";
        case 'b':   return "blue";
        default:    return {};
    }
}

inline std::optional<float> morphPoleForKeyboardShortcut(
        const juce::KeyPress& key) {
    if (key.getModifiers().isAnyModifierKeyDown()) {
        return std::nullopt;
    }
    if (key.getKeyCode() == juce::KeyPress::leftKey) {
        return 0.f;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey) {
        return 1.f;
    }
    return std::nullopt;
}

inline bool isMorphLinkKeyboardShortcut(const juce::KeyPress& key) {
    return !key.getModifiers().isAnyModifierKeyDown()
            && juce::CharacterFunctions::toLowerCase(key.getKeyCode()) == 'l';
}

inline bool isMorphKeyboardShortcut(const juce::KeyPress& key) {
    return morphAxisForKeyboardShortcut(key).isNotEmpty()
            || morphPoleForKeyboardShortcut(key).has_value()
            || isMorphLinkKeyboardShortcut(key);
}

}
