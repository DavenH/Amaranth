#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class PresetMetadataStore {
public:
    static juce::StringArray normalize(juce::StringArray tags);
    static bool save(
            const juce::File& file,
            juce::StringArray tags,
            juce::String& error);
    static bool saveTitle(
            const juce::File& file,
            const juce::String& title,
            juce::String& error);
};

}
