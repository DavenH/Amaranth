#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct TrimeshSemanticRepresentationMigrationResult {
    bool migrated {};
    juce::String error;

    bool succeeded() const { return error.isEmpty(); }
};

class TrimeshSemanticRepresentationMigration {
public:
    TrimeshSemanticRepresentationMigrationResult migrate(juce::var& graph) const;
};

}
