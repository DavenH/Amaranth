#pragma once

#include <JuceHeader.h>
#include <vector>

namespace CycleV2 {

class SpyPlaybackRipple {
public:
    static void apply(juce::Image& image, const std::vector<float>& positions);
};

}
