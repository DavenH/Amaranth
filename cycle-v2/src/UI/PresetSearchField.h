#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class PresetSearchField final : public juce::TextEditor {
public:
    explicit PresetSearchField(const juce::String& placeholder);

    void setPlaybackToggleCallback(std::function<void()> callback);
    bool keyPressed(const juce::KeyPress& key) override;

private:
    std::function<void()> onPlaybackToggle;
};

}
