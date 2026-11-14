#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class OutputStageRepresentationMigration {
public:
    juce::Result migrate(juce::var& graph) const;
};

}
