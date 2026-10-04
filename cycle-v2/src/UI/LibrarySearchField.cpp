#include "UI/LibrarySearchField.h"

#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

LibrarySearchField::LibrarySearchField(const juce::String& placeholder) {
    addKeyListener(this);
    setTextToShowWhenEmpty(placeholder, CanvasChromePalette::mutedText);
    setColour(backgroundColourId, CanvasChromePalette::restingControlSurface);
    setColour(outlineColourId, CanvasChromePalette::border);
    setColour(focusedOutlineColourId, CanvasChromePalette::navigationAccent);
    setColour(textColourId, CanvasChromePalette::text);
    setFont(juce::FontOptions(15.f));
    setIndents(36, 8);
}

void LibrarySearchField::setPlaybackToggleCallback(std::function<void()> callback) {
    onPlaybackToggle = std::move(callback);
}

void LibrarySearchField::paintOverChildren(juce::Graphics& graphics) {
    const juce::Rectangle<float> lens(
            14.f, (float) getHeight() * 0.5f - 5.f, 9.f, 9.f);
    graphics.setColour(CanvasChromePalette::mutedText.withAlpha(0.82f));
    graphics.drawEllipse(lens, 1.25f);
    graphics.drawLine(lens.getRight() - 1.f, lens.getBottom() - 1.f,
            lens.getRight() + 3.f, lens.getBottom() + 3.f, 1.25f);
}

bool LibrarySearchField::keyPressed(const juce::KeyPress& key) {
    if (handlePlaybackSpace(key)) {
        return true;
    }
    return juce::TextEditor::keyPressed(key);
}

bool LibrarySearchField::keyPressed(const juce::KeyPress& key, juce::Component*) {
    return handlePlaybackSpace(key);
}

void LibrarySearchField::insertTextAtCaret(const juce::String& text) {
    // macOS commits printable keys through TextInputTarget before keyPressed.
    if (text == " " && getText().trim().isEmpty()) {
        handlePlaybackSpace(juce::KeyPress(' ', juce::ModifierKeys::noModifiers, ' '));
        return;
    }
    juce::TextEditor::insertTextAtCaret(text);
}

bool LibrarySearchField::handlePlaybackSpace(const juce::KeyPress& key) {
    const auto modifiers = key.getModifiers();
    if ((key.getKeyCode() == juce::KeyPress::spaceKey
                    || key.getTextCharacter() == ' ')
            && !modifiers.isCommandDown()
            && !modifiers.isCtrlDown()
            && !modifiers.isAltDown()
            && getText().trim().isEmpty()) {
        if (getText().isNotEmpty()) {
            clear();
        }
        if (onPlaybackToggle) {
            onPlaybackToggle();
        }
        return true;
    }
    return false;
}

}
