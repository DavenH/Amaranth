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

void GLPanelRenderer::drawLine(float x1, float y1, float x2, float y2, bool scale) {
    if (gfx != nullptr) {
        gfx->drawLine(x1, y1, x2, y2, scale);
    }
}

void GLPanelRenderer::drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) {
    if (gfx != nullptr) {
        gfx->drawLine(x1, y1, x2, y2, c1, c2);
    }
}

void GLPanelRenderer::drawPoint(float size, Vertex2 point, bool scale) {
    if (gfx != nullptr) {
        gfx->drawPoint(size, point, scale);
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

void GLPanelRenderer::drawTexture(Texture* texture) {
    if (gfx != nullptr) {
        gfx->drawTexture(texture);
    }
}

void GLPanelRenderer::enableSmoothing() {
    if (gfx != nullptr) {
        gfx->enableSmoothing();
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

void GLPanelRenderer::disableSmoothing() {
    if (gfx != nullptr) {
        gfx->disableSmoothing();
    }
}

void GLPanelRenderer::setCurrentColour(const Color& color) {
    if (gfx != nullptr) {
        gfx->setCurrentColour(color);
    }
}

void GLPanelRenderer::setCurrentColour(float c1, float c2, float c3) {
    if (gfx != nullptr) {
        gfx->setCurrentColour(c1, c2, c3);
    }
}

void GLPanelRenderer::setCurrentColour(float c1, float c2, float c3, float c4) {
    if (gfx != nullptr) {
        gfx->setCurrentColour(c1, c2, c3, c4);
    }
}

void GLPanelRenderer::setCurrentLineWidth(float width) {
    if (gfx != nullptr) {
        gfx->setCurrentLineWidth(width);
    }
}

void GLPanelRenderer::setClip(const juce::Rectangle<int>& clip) {
    ignoreUnused(clip);
}

void GLPanelRenderer::setTransform(const juce::AffineTransform& transform) {
    ignoreUnused(transform);
}
