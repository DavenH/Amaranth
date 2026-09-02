#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class EffectEnableButton final : public juce::ToggleButton {
public:
    EffectEnableButton();

    void paintButton(
            juce::Graphics& graphics,
            bool highlighted,
            bool down) override;
};

}
