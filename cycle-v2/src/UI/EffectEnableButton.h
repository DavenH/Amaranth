#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class EffectEnableButton final : public juce::ToggleButton {
public:
    explicit EffectEnableButton(
            const juce::String& title = "Effect enabled",
            const juce::String& description = "Toggles processing for this effect",
            const juce::String& tooltip = "Enable or bypass effect");

    void paintButton(
            juce::Graphics& graphics,
            bool highlighted,
            bool down) override;
};

}
