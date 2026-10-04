#include "UI/PresetSearchField.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

PresetSearchField::PresetSearchField(const juce::String& placeholder) {
    setTextToShowWhenEmpty(placeholder, CanvasChromePalette::mutedText);
    setColour(backgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(outlineColourId, CanvasChromePalette::border);
    setColour(focusedOutlineColourId, CanvasChromePalette::navigationAccent);
    setColour(textColourId, CanvasChromePalette::text);
    setFont(juce::FontOptions(15.f));
    setIndents(36, 8);
}

void PresetSearchField::setPlaybackToggleCallback(std::function<void()> callback) {
    onPlaybackToggle = std::move(callback);
}

bool PresetSearchField::keyPressed(const juce::KeyPress& key) {
    const auto modifiers = key.getModifiers();
    if (key.getKeyCode() == juce::KeyPress::spaceKey
            && !modifiers.isCommandDown()
            && !modifiers.isCtrlDown()
            && !modifiers.isAltDown()
            && getText().isEmpty()) {
        if (onPlaybackToggle) {
            onPlaybackToggle();
        }
        return true;
    }
    return juce::TextEditor::keyPressed(key);
}

}
