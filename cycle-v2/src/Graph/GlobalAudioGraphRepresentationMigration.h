#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

struct GlobalAudioGraphRepresentationMigrationResult {
    bool migrated {};
    juce::String error;
    std::vector<juce::String> globalNodeIds;

    bool succeeded() const { return error.isEmpty(); }
};

class GlobalAudioGraphRepresentationMigration {
public:
    GlobalAudioGraphRepresentationMigrationResult migrate(juce::var& graph) const;
};

}
