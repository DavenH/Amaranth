#pragma once

#include "UI/PresetBrowserLookAndFeel.h"

namespace CycleV2 {

class DialogLookAndFeel final : public PresetBrowserLookAndFeel {
public:
    DialogLookAndFeel();
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
            bool, bool) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    int getAlertWindowButtonHeight() override;
    void drawAlertBox(juce::Graphics&, juce::AlertWindow&,
            const juce::Rectangle<int>&, juce::TextLayout&) override;
    void fillResizableWindowBackground(juce::Graphics&, int, int,
            const juce::BorderSize<int>&, juce::ResizableWindow&) override;
    void drawProgressBar(juce::Graphics&, juce::ProgressBar&, int, int,
            double, const juce::String&) override;
    static void markPrimary(juce::Button&);
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
