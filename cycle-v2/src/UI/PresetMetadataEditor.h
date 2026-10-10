#pragma once

#include <JuceHeader.h>

#include <functional>

namespace CycleV2 {

class PresetMetadataEditor {
public:
    using TagsSaved = std::function<void(const juce::StringArray&)>;
    using TitleSaved = std::function<void(const juce::String&)>;

    static void editTags(
            juce::Component& owner,
            const juce::File& file,
            const juce::StringArray& currentTags,
            TagsSaved onSaved);
    static void rename(
            juce::Component& owner,
            const juce::File& file,
            const juce::String& currentTitle,
            TitleSaved onSaved);
};

}
