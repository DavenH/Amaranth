#pragma once

#include "JuceHeader.h"

class ScopedGLScissor {
public:
    ScopedGLScissor(juce::Rectangle<float> bounds, float scaleFactor);
    ~ScopedGLScissor();

    static juce::Rectangle<int> boxFor(
            juce::Rectangle<float> bounds,
            float scaleFactor,
            juce::Rectangle<int> viewport);

    ScopedGLScissor(const ScopedGLScissor&) = delete;
    ScopedGLScissor& operator=(const ScopedGLScissor&) = delete;

private:
    GLboolean wasEnabled {};
    GLint previousBox[4] {};
};
