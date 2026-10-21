#pragma once

#include "JuceHeader.h"

namespace CycleV2 {

class GraphCommandDispatcher;
class GraphDocument;

class ImpulseResponseImportAutomation {
public:
    static juce::var apply(
            GraphDocument& document,
            GraphCommandDispatcher& commands,
            const juce::var& request,
            const juce::File& file,
            juce::String& error);
};

}
