#pragma once

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

bool decodeNodeAuthoringEdits(const juce::var& command,
        NodeAuthoringEdits& request, juce::String& error);
juce::var nodeAuthoringResult(const GraphEditResult& result, uint64_t documentRevision);

}
