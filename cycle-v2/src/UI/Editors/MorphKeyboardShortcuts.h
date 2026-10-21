#pragma once

#include <JuceHeader.h>

#include <optional>

namespace CycleV2 {

inline juce::String morphKeyboardShortcutHelpText(bool includesYellowAxis) {
    const juce::String focusKeys = includesYellowAxis
            ? "Y / R / B"
            : "R / B";
    return focusKeys + ": Focus group\n"
            "Up / Down: Move focus\n"
            "Left / Right: Send morph to pole\n"
            "Shift + Left / Right: Move morph 20%\n"
            "L: Toggle pole lock";
}

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

inline std::optional<int> morphAxisNavigationForKeyboardShortcut(
        const juce::KeyPress& key) {
    if (key.getModifiers().isAnyModifierKeyDown()) {
        return std::nullopt;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey) {
        return -1;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey) {
        return 1;
    }
    return std::nullopt;
}

inline std::optional<float> morphStepForKeyboardShortcut(
        const juce::KeyPress& key) {
    const juce::ModifierKeys modifiers = key.getModifiers();
    if (!modifiers.isShiftDown()
            || modifiers.isCommandDown()
            || modifiers.isCtrlDown()
            || modifiers.isAltDown()) {
        return std::nullopt;
    }

    if (key.getKeyCode() == juce::KeyPress::leftKey) {
        return -0.2f;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey) {
        return 0.2f;
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
            || morphAxisNavigationForKeyboardShortcut(key).has_value()
            || morphStepForKeyboardShortcut(key).has_value()
            || isMorphLinkKeyboardShortcut(key);
}

}
