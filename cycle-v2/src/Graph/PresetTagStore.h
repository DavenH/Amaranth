#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class PresetTagStore {
public:
    static juce::StringArray normalize(juce::StringArray tags);
    static bool save(
            const juce::File& file,
            juce::StringArray tags,
            juce::String& error);
};

}
