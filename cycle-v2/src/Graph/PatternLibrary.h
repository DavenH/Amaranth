#pragma once

#include <JuceHeader.h>

#include <optional>
#include <vector>

#include "Graph/PresetPresentation.h"

namespace CycleV2 {

struct PatternRecord {
    juce::String id;
    juce::String name;
    PresetMidiSequence sequence;
    juce::File file;
    bool factory {};
    juce::String tag;
    juce::StringArray tags;
};

class PatternLibrary {
public:
    PatternLibrary(juce::File factoryDirectory, juce::File userDirectory);

    void reload();
    const std::vector<PatternRecord>& records() const { return patterns; }
    const PatternRecord* find(const juce::String& id) const;
    std::optional<PatternRecord> saveUserPattern(
            const juce::String& id,
            const juce::String& name,
            const PresetMidiSequence& sequence,
            const juce::String& tag = {},
            const juce::StringArray& tags = {});
    juce::String newUserId() const;

private:
    void readDirectory(const juce::File& directory, bool factory);

    juce::File factoryDirectory;
    juce::File userDirectory;
    std::vector<PatternRecord> patterns;
};

}
