#pragma once

#include <cstdint>
#include <unordered_map>

#include "JuceHeader.h"

class CommonGfx;
class Texture;

class RenderResourceCache {
public:
    void drawCachedTexture(CommonGfx* gfx, Texture* texture, const juce::Rectangle<float>& bounds);
    void drawTexture(CommonGfx* gfx, Texture* texture);
    uint32_t getVersion(Texture* texture) const;
    bool isTracked(Texture* texture) const;
    void updateTexture(CommonGfx* gfx, Texture* texture);

private:
    uint32_t touch(Texture* texture);

    std::unordered_map<Texture*, uint32_t> textureVersions;
};
