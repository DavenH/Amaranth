#pragma once

#include "JuceHeader.h"

namespace CycleV2 {

inline juce::String formatTrimeshMorphValue(float value) {
    return juce::String(juce::jlimit(0.f, 1.f, value), 3);
}

}
