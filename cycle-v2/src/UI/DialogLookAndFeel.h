#pragma once

#include <UI/AmaranthLookAndFeel.h>

namespace CycleV2 {

class DialogLookAndFeel final : public AmaranthLookAndFeel {
public:
    DialogLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
            bool, bool) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&,
            const juce::Rectangle<int>&, juce::TextLayout&) override;
};

class StyledAlertWindow final : public juce::AlertWindow {
public:
    StyledAlertWindow(const juce::String& title, const juce::String& message,
            juce::Component* associated);
    ~StyledAlertWindow() override;

private:
    DialogLookAndFeel theme;
};

}
