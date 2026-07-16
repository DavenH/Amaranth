#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

struct Effect2DVertexState {
    float x {};
    float y {};
    float curve {};
};

class Effect2DMeshState {
public:
    static std::vector<Effect2DVertexState> parse(const juce::String& serialized);
    static juce::String serialize(const std::vector<Effect2DVertexState>& vertices);
};

}
