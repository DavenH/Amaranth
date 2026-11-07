#pragma once

#include "UI/SpyFrameRenderer.h"

namespace CycleV2 {

class SpyFrameAutomation {
public:
    static juce::var gradients();
    static std::optional<SpyFrameRequest> readRequest(const juce::var& command, String& error);
    static juce::var writeFrame(const SpyFrameResult& frame, const File& destination);
};

}
