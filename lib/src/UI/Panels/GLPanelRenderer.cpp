#include "GLPanelRenderer.h"

#include "CommonGfx.h"
#include "PanelRenderContext.h"
#include "GLSurfaceCache.h"
#include "Texture.h"

using namespace gl;

GLPanelRenderer::GLPanelRenderer(CommonGfx* gfx, GLSurfaceCache* surfaceCache) :
        gfx(gfx)
    ,   surfaceCache(surfaceCache)
{
}

void GLPanelRenderer::beginPanelRender(const PanelRenderContext& context) {
    currentContext = &context;

    if (gfx != nullptr) {
        gfx->initRender();
    }
}

void GLPanelRenderer::checkErrors() {
    if (gfx != nullptr) {
        gfx->checkErrors();
    }
}

void GLPanelRenderer::endPanelRender() {
    currentContext = nullptr;
}

void GLPanelRenderer::finishSurfaceGrid() {
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void GLPanelRenderer::drawBackground(const juce::Rectangle<int>& bounds, bool fillBackground) {
    if (gfx != nullptr) {
        gfx->drawBackground(bounds, fillBackground);
    }
}

void GLPanelRenderer::drawCachedTexture(Texture* texture, const juce::Rectangle<float>& bounds) {
    resourceCache.drawCachedTexture(gfx, texture, bounds);
}

void GLPanelRenderer::drawSurfaceColumn(Buffer<Int8u> colours, Buffer<float> vertices, int stride, int sizeY) {
    ignoreUnused(sizeY);

    glColorPointer(stride, GL_UNSIGNED_BYTE, 0, colours);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_QUAD_STRIP, 0, (sizeY + 1) * 2);
}

void GLPanelRenderer::drawSurfaceCache() {
    if (surfaceCache != nullptr) {
        surfaceCache->draw();
    }
}

void GLPanelRenderer::drawFinalSelection() {
    if (gfx != nullptr) {
        gfx->drawFinalSelection();
    }
}

void GLPanelRenderer::finishSurfaceBake() {
    if (surfaceCache != nullptr && currentContext != nullptr) {
        surfaceCache->captureFromFramebuffer(currentContext->bounds.getHeight());
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
    resourceCache.drawTexture(gfx, texture);
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

void GLPanelRenderer::beginSurfaceGrid() {
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
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

void GLPanelRenderer::updateTexture(Texture* texture) {
    resourceCache.updateTexture(gfx, texture);
}
