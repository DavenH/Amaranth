#pragma once

#include <JuceHeader.h>

#include "Graph/GraphCompiler.h"

namespace CycleV2 {

class OfflineAudioCaptureAutomation {
public:
    static bool isScheduledCapture(const juce::var& command);
    static bool capture(
            const juce::var& command,
            const juce::File& path,
            GraphExecutionPlan plan,
            uint64_t revision,
            juce::var& data,
            juce::String& error);
};

}
