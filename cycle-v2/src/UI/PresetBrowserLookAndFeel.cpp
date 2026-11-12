#include "UI/PresetBrowserLookAndFeel.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/LibraryIconRenderer.h"

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
    if ((button.getProperties().contains("libraryIcon")
                || (bool) button.getProperties()["quietAction"])
            && !isHighlighted && !isDown && !button.hasKeyboardFocus(false)) {
        return;
    }
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
    if (LibraryIconRenderer::paintButton(graphics, button)) {
        return;
    }
    juce::Colour text = button.findColour(button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId);
    if (!button.isEnabled()) {
        text = text.withMultipliedAlpha(0.45f);
    }
    graphics.setColour(text);
    graphics.setFont(getTextButtonFont(button, button.getHeight()));
    if ((bool) button.getProperties()["disclosure"]) {
        const float x = 9.f;
        const float y = button.getHeight() * 0.5f;
        juce::Path arrow;
        if (button.getToggleState()) {
            arrow.addTriangle(x - 3.f, y - 2.f, x + 3.f, y - 2.f, x, y + 2.f);
        } else {
            arrow.addTriangle(x - 2.f, y - 3.f, x - 2.f, y + 3.f, x + 2.f, y);
        }
        graphics.fillPath(arrow);
        graphics.drawFittedText(button.getButtonText(),
                button.getLocalBounds().withTrimmedLeft(21).withTrimmedRight(6),
                juce::Justification::centredLeft, 1);
        return;
    }
    graphics.drawFittedText(
            button.getButtonText(),
            button.getLocalBounds().reduced((bool) button.getProperties().getWithDefault("sidebarTab", false) ? 2 : 10, 0),
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
