#include "GLPanelRenderer.h"

#include "CommonGfx.h"
#include "Texture.h"

GLPanelRenderer::GLPanelRenderer(CommonGfx* gfx) :
        gfx(gfx)
{
}

void GLPanelRenderer::beginPanelRender(const PanelRenderContext& context) {
    ignoreUnused(context);

    if (gfx != nullptr) {
        gfx->initRender();
    }
}

void GLPanelRenderer::endPanelRender() {
}

void GLPanelRenderer::drawCachedTexture(Texture* texture, const juce::Rectangle<float>& bounds) {
    if (gfx != nullptr) {
        gfx->drawSubTexture(texture, bounds);
    }
}

void GLPanelRenderer::drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) {
    if (gfx != nullptr) {
        gfx->drawLine(x1, y1, x2, y2, c1, c2);
    }
}

void GLPanelRenderer::drawLineStrip(BufferXY& xy, bool scale) {
    if (gfx != nullptr) {
        gfx->drawLineStrip(xy, true, scale);
    }
}

void GLPanelRenderer::drawPoints(float size, BufferXY& xy, bool scale) {
    if (gfx != nullptr) {
        gfx->drawPoints(size, xy, scale);
    }
}

void GLPanelRenderer::drawPoints(float size, BufferXY& xy, Buffer<float> c, bool scale) {
    if (gfx != nullptr) {
        gfx->drawPoints(size, xy, c, scale);
    }
}

void GLPanelRenderer::drawRect(float leftX, float topY, float rightX, float botY, bool scale) {
    if (gfx != nullptr) {
        gfx->drawRect(leftX, topY, rightX, botY, scale);
    }
}

void GLPanelRenderer::fillAndOutlineColoured(
    const std::vector<ColorPos>& positions,
    float baseY,
    float baseAlpha,
    bool fill,
    bool outline
) {
    if (gfx != nullptr) {
        gfx->fillAndOutlineColoured(positions, baseY, baseAlpha, fill, outline);
    }
}

void GLPanelRenderer::fillRect(float leftX, float topY, float rightX, float botY, bool scale) {
    if (gfx != nullptr) {
        gfx->fillRect(leftX, topY, rightX, botY, scale);
    }
}

void GLPanelRenderer::fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) {
    if (gfx != nullptr) {
        gfx->fillRect(x1, y1, x2, y2, c1, c2);
    }
}

void GLPanelRenderer::setClip(const juce::Rectangle<int>& clip) {
    ignoreUnused(clip);
}

void GLPanelRenderer::setTransform(const juce::AffineTransform& transform) {
    ignoreUnused(transform);
}
