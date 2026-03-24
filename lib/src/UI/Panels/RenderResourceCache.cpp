#include "RenderResourceCache.h"

#include "CommonGfx.h"
#include "Texture.h"

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

bool RenderResourceCache::isTracked(Texture* texture) const {
    return textureVersions.find(texture) != textureVersions.end();
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
