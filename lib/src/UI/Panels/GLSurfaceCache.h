#pragma once

#include "JuceHeader.h"
#include "Texture.h"

class GLSurfaceCache {
public:
    void allocate(bool transparent);
    void captureFromFramebuffer(int componentHeight);
    void clear();
    void create();
    void draw() const;
    bool paintSnapshot(juce::Graphics& g, const juce::Rectangle<int>& bounds) const;
    void setSize(int width, int height);

private:
    TextureGL texture;
    bool transparent = false;
    mutable juce::CriticalSection snapshotLock;
    juce::Image snapshot;
};
