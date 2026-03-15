#pragma once

#include "PanelRenderer.h"
#include "RenderResourceCache.h"

#include "../../Obj/Ref.h"

class CommonGfx;
class GLSurfaceCache;

class GLPanelRenderer :
        public PanelRenderer {
public:
    explicit GLPanelRenderer(CommonGfx* gfx, GLSurfaceCache* surfaceCache = nullptr);

    void beginPanelRender(const PanelRenderContext& context) override;
    void checkErrors() override;
    void endPanelRender() override;

    void drawBackground(const juce::Rectangle<int>& bounds, bool fillBackground) override;
    void drawCachedTexture(Texture* texture, const juce::Rectangle<float>& bounds) override;
    void drawSurfaceCache() override;
    void drawFinalSelection() override;
    void finishSurfaceBake() override;
    void drawLine(float x1, float y1, float x2, float y2, bool scale) override;
    void drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
    void drawPoint(float size, Vertex2 point, bool scale) override;
    void drawLineStrip(BufferXY& xy, bool scale) override;
    void drawPoints(float size, BufferXY& xy, bool scale) override;
    void drawPoints(float size, BufferXY& xy, Buffer<float> c, bool scale) override;
    void drawRect(float leftX, float topY, float rightX, float botY, bool scale) override;
    void fillAndOutlineColoured(
        const std::vector<ColorPos>& positions,
        float baseY,
        float baseAlpha,
        bool fill,
        bool outline
    ) override;
    void drawTexture(Texture* texture) override;
    void enableSmoothing() override;
    void fillRect(float leftX, float topY, float rightX, float botY, bool scale) override;
    void fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
    void disableSmoothing() override;
    void setCurrentColour(const Color& color) override;
    void setCurrentColour(float c1, float c2, float c3) override;
    void setCurrentColour(float c1, float c2, float c3, float c4) override;
    void setCurrentLineWidth(float width) override;
    void setClip(const juce::Rectangle<int>& clip) override;
    void setTransform(const juce::AffineTransform& transform) override;
    void updateTexture(Texture* texture) override;

private:
    Ref<CommonGfx> gfx;
    GLSurfaceCache* surfaceCache;
    const PanelRenderContext* currentContext = nullptr;
    RenderResourceCache resourceCache;
};
