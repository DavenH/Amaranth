#pragma once

#include <JuceHeader.h>

#include <array>

namespace CycleV2 {

struct TrimeshGuideAttachmentTarget {
    int vertexIndex { -1 };
    juce::String field;

    bool isValid() const;
    int fieldIndex() const;

    static constexpr int fieldCount = 6;
    static const std::array<juce::String, fieldCount>& fields();
    static TrimeshGuideAttachmentTarget parse(const juce::String& portId);
    static juce::String portIdFor(int vertexIndex, const juce::String& field);
};

}
