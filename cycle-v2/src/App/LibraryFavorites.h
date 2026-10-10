#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class LibraryFavorites final {
public:
    LibraryFavorites(juce::PropertiesFile& properties, juce::File factoryPresetDirectory);

    bool isPresetFavorite(const juce::File& file) const;
    bool isPatternFavorite(const juce::String& id) const;
    bool togglePreset(const juce::File& file);
    bool togglePattern(const juce::String& id);

private:
    juce::String presetKey(const juce::File& file) const;
    bool contains(const juce::String& key) const;
    bool toggle(const juce::String& key);

    juce::PropertiesFile& properties;
    juce::File factoryPresetDirectory;
    juce::StringArray keys;
};

}
