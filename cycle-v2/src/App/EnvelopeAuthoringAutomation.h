#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class GraphDocument;
class GraphCommandDispatcher;

juce::var runEnvelopeAuthoringCommand(GraphDocument& document,
        GraphCommandDispatcher& commands, const juce::var& request);

}
