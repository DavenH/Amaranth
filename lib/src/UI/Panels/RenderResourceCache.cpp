#include "RenderResourceCache.h"

#include "CommonGfx.h"
#include "Texture.h"

void RenderResourceCache::clear() {
    textureVersions.clear();
    categoryVersions.fill(0);
}

void RenderResourceCache::clearDirtyFlagAfterRebuild(
    PanelDirtyState& dirtyState,
    InvalidationCategory category
) const {
    switch (category) {
        case InvalidationCategory::StaticVisual:
            dirtyState.clear(PanelDirtyState::Flag::StaticVisual);
            break;
        case InvalidationCategory::SurfaceData:
            dirtyState.clear(PanelDirtyState::Flag::SurfaceCache);
            break;
        case InvalidationCategory::Transform:
            dirtyState.clear(PanelDirtyState::Flag::Layout);
            break;
        case InvalidationCategory::FullRebuild:
            dirtyState.clear(PanelDirtyState::Flag::Full);
            break;
        case InvalidationCategory::Count:
            break;
    }
}

void RenderResourceCache::drawCachedTexture(CommonGfx* gfx, Texture* texture, const juce::Rectangle<float>& bounds) {
    if (gfx == nullptr || texture == nullptr) {
        return;
    }

    touch(texture);
    gfx->drawSubTexture(texture, bounds);
}

void RenderResourceCache::drawTexture(CommonGfx* gfx, Texture* texture) {
    if (gfx == nullptr || texture == nullptr) {
        return;
    }

    touch(texture);
    gfx->drawTexture(texture);
}

uint32_t RenderResourceCache::getVersion(Texture* texture) const {
    auto iter = textureVersions.find(texture);
    return iter == textureVersions.end() ? 0u : iter->second;
}

uint32_t RenderResourceCache::getInvalidationVersion(InvalidationCategory category) const {
    return categoryVersions[categoryIndex(category)];
}

void RenderResourceCache::invalidate(InvalidationCategory category) {
    if (category == InvalidationCategory::Count) {
        return;
    }

    ++categoryVersions[categoryIndex(category)];
}

bool RenderResourceCache::isTracked(Texture* texture) const {
    return textureVersions.find(texture) != textureVersions.end();
}

void RenderResourceCache::markTextureUpdated(Texture* texture) {
    if (texture != nullptr) {
        touch(texture);
    }
}

void RenderResourceCache::updateTexture(CommonGfx* gfx, Texture* texture) {
    if (gfx == nullptr || texture == nullptr) {
        return;
    }

    touch(texture);
    gfx->updateTexture(texture);
}

uint32_t RenderResourceCache::touch(Texture* texture) {
    auto iter = textureVersions.find(texture);
    if (iter == textureVersions.end()) {
        iter = textureVersions.emplace(texture, 0u).first;
    }

    return ++iter->second;
}
