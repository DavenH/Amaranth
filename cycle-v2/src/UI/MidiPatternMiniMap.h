#pragma once

#include <JuceHeader.h>

#include "Graph/PresetPresentation.h"

namespace CycleV2::MidiPatternMiniMap {

juce::Range<int> pitchRange(
        const PresetMidiSequence& sequence, int minimumSpan, int margin);
void paintNotes(
        juce::Graphics& graphics,
        const PresetMidiSequence& sequence,
        juce::Rectangle<float> bounds,
        juce::Range<int> pitches);
void paintControls(
        juce::Graphics& graphics,
        const PresetMidiSequence& sequence,
        juce::Rectangle<float> bounds);

}
