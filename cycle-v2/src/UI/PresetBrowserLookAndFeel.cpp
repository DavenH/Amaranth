#include "UI/PresetBrowserLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

juce::Font PresetBrowserLookAndFeel::getTextButtonFont(
        juce::TextButton&,
        int) {
    return juce::Font(juce::FontOptions(11.5f).withStyle("Bold"));
}

void PresetBrowserLookAndFeel::drawButtonBackground(
        juce::Graphics& graphics,
        juce::Button& button,
        const juce::Colour& backgroundColour,
        bool isHighlighted,
        bool isDown) {
    juce::Colour surface = backgroundColour;
    if (isDown) {
        surface = surface.darker(0.18f);
    } else if (isHighlighted) {
        surface = surface.brighter(0.10f);
    }
    if (!button.isEnabled()) {
        surface = surface.withMultipliedAlpha(0.45f);
    }

    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    graphics.setColour(surface);
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(button.findColour(juce::TextButton::buttonOnColourId));
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::controlCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
}

void PresetBrowserLookAndFeel::drawButtonText(
        juce::Graphics& graphics,
        juce::TextButton& button,
        bool,
        bool) {
    juce::Colour text = button.findColour(button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId);
    if (!button.isEnabled()) {
        text = text.withMultipliedAlpha(0.45f);
    }
    graphics.setColour(text);
    graphics.setFont(getTextButtonFont(button, button.getHeight()));
    graphics.drawFittedText(
            button.getButtonText(),
            button.getLocalBounds().reduced(10, 0),
            juce::Justification::centred,
            1);
}

void PresetBrowserLookAndFeel::fillTextEditorBackground(
        juce::Graphics& graphics,
        int width,
        int height,
        juce::TextEditor& editor) {
    graphics.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    graphics.fillRoundedRectangle(
            juce::Rectangle<float>(0.f, 0.f, (float) width, (float) height),
            CanvasChromeMetrics::controlCornerRadius);

    const juce::Rectangle<float> lens(14.f, (float) height * 0.5f - 5.f, 9.f, 9.f);
    graphics.setColour(CanvasChromePalette::mutedText.withAlpha(0.82f));
    graphics.drawEllipse(lens, 1.25f);
    graphics.drawLine(
            lens.getRight() - 1.f,
            lens.getBottom() - 1.f,
            lens.getRight() + 3.f,
            lens.getBottom() + 3.f,
            1.25f);
}

void PresetBrowserLookAndFeel::drawTextEditorOutline(
        juce::Graphics& graphics,
        int width,
        int height,
        juce::TextEditor& editor) {
    const bool focused = editor.hasKeyboardFocus(true);
    graphics.setColour(editor.findColour(focused
            ? juce::TextEditor::focusedOutlineColourId
            : juce::TextEditor::outlineColourId));
    graphics.drawRoundedRectangle(
            juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.f, (float) height - 1.f),
            CanvasChromeMetrics::controlCornerRadius,
            focused
                    ? CanvasChromeMetrics::focusRingWidth
                    : CanvasChromeMetrics::restingBorderWidth);
}

}
