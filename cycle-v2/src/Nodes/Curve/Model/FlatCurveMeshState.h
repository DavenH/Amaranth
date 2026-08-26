#pragma once

#include <JuceHeader.h>

#include <vector>

namespace CycleV2 {

struct FlatCurveVertexState {
    float x {};
    float y {};
    float curve {};
};

class FlatCurveMeshState {
public:
    static std::vector<FlatCurveVertexState> parse(const juce::String& serialized);
    static juce::String serialize(const std::vector<FlatCurveVertexState>& vertices);
};

}
