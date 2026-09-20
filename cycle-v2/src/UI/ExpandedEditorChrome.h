#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

#include "UI/EffectEnableButton.h"

namespace CycleV2 {

class ExpandedEditorChrome {
public:
    ExpandedEditorChrome(
            juce::Component& owner,
            juce::String title,
            juce::String componentPrefix,
            std::function<void()> close,
            std::function<void(bool)> enabled = {});

    void paint(juce::Graphics& graphics) const;
    void resized();
    void setEnabled(bool enabled);
    bool isEnabled() const;

private:
    juce::Component& owner;
    juce::String title;
    juce::TextButton closeButton;
    std::unique_ptr<EffectEnableButton> enabledButton;
};

}
