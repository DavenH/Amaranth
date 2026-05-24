#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <unordered_map>

#include "JuceHeader.h"
#include "PanelDirtyState.h"

class CommonGfx;
class Texture;

class RenderResourceCache {
public:
    enum class InvalidationCategory {
        StaticVisual,
        SurfaceData,
        Transform,
        FullRebuild,
        Count
    };

    void clear();
    void clearDirtyFlagAfterRebuild(PanelDirtyState& dirtyState, InvalidationCategory category) const;
    void drawCachedTexture(CommonGfx* gfx, Texture* texture, const juce::Rectangle<float>& bounds);
    void drawTexture(CommonGfx* gfx, Texture* texture);
    uint32_t getInvalidationVersion(InvalidationCategory category) const;
    uint32_t getVersion(Texture* texture) const;
    void invalidate(InvalidationCategory category);
    bool isTracked(Texture* texture) const;
    void markTextureUpdated(Texture* texture);
    void updateTexture(CommonGfx* gfx, Texture* texture);

private:
    static constexpr size_t numInvalidationCategories =
        static_cast<size_t>(InvalidationCategory::Count);

    static constexpr size_t categoryIndex(InvalidationCategory category) {
        return static_cast<size_t>(category);
    }

    uint32_t touch(Texture* texture);

    std::array<uint32_t, numInvalidationCategories> categoryVersions {};
    std::unordered_map<Texture*, uint32_t> textureVersions;
};
