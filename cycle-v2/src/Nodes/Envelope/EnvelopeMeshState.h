#pragma once

#include <JuceHeader.h>

class EnvelopeMesh;

namespace CycleV2 {

enum class EnvelopePurpose : int;

class EnvelopeMeshState {
public:
    static void initialiseDefault(EnvelopeMesh& mesh);
    static void initialiseDefault(EnvelopeMesh& mesh, EnvelopePurpose purpose);
    static juce::String defaultSnapshot();
    static juce::String serialize(const EnvelopeMesh& mesh);
    static bool apply(const juce::String& serialized, EnvelopeMesh& mesh);
};

}
