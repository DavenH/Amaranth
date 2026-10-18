#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class PreviewMorphControl {
    KeyScale,
    ModWheel,
    MidiController
};

struct PreviewMorphTarget {
    juce::String nodeId;
    juce::String parameterId;
    PreviewMorphControl control { PreviewMorphControl::KeyScale };
    int controller { 1 };
};

}
