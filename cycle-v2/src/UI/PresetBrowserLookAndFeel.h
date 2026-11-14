#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class PresetBrowserLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    void drawButtonBackground(
            juce::Graphics& graphics,
            juce::Button& button,
            const juce::Colour& backgroundColour,
            bool isHighlighted,
            bool isDown) override;
    void drawButtonText(
            juce::Graphics& graphics,
            juce::TextButton& button,
            bool isHighlighted,
            bool isDown) override;
    void fillTextEditorBackground(
            juce::Graphics& graphics,
            int width,
            int height,
            juce::TextEditor& editor) override;
    void drawTextEditorOutline(
            juce::Graphics& graphics,
            int width,
            int height,
            juce::TextEditor& editor) override;
};

}
