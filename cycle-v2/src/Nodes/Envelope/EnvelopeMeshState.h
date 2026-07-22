#pragma once

#include <JuceHeader.h>

class EnvelopeMesh;

namespace CycleV2 {

class EnvelopeMeshState {
public:
    static void initialiseDefault(EnvelopeMesh& mesh);
    static juce::String defaultSnapshot();
    static juce::String serialize(const EnvelopeMesh& mesh);
    static bool apply(const juce::String& serialized, EnvelopeMesh& mesh);
};

}
