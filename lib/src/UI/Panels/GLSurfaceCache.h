#pragma once

#include "JuceHeader.h"
#include "Texture.h"

class GLSurfaceCache {
public:
    void allocate(bool transparent);
    void captureFromFramebuffer(const juce::Rectangle<int>& componentBounds);
    void clear();
    void create();
    void draw() const;
    bool paintSnapshot(juce::Graphics& g, const juce::Rectangle<int>& bounds) const;
    void setSize(int width, int height);
    void setRenderScale(double scale);

private:
    void updatePixelBounds();

    TextureGL texture;
    juce::Rectangle<int> activeBounds;
    juce::Rectangle<int> activePixelBounds;
    double renderScale = 1.0;
    bool transparent = false;
    mutable juce::CriticalSection snapshotLock;
    juce::Image snapshot;
};
