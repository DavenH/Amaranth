#pragma once

#include <JuceHeader.h>

class Mesh;

namespace CycleV2 {

class TrimeshMeshState {
public:
    static const juce::String& parameterId();
    static juce::String serialize(const Mesh& mesh);
    static bool apply(const juce::String& serialized, Mesh& mesh);
    static bool isValid(const juce::String& serialized);
};

}
