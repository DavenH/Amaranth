#pragma once

#include <JuceHeader.h>

namespace CycleV2 {
class GraphDocument;
class GraphCommandDispatcher;
juce::var runMeshTopologyCommand(GraphDocument&, GraphCommandDispatcher&, const juce::var&);
}
