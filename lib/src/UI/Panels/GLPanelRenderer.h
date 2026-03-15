#pragma once

#include "PanelRenderer.h"

#include "../../Obj/Ref.h"

class CommonGfx;

class GLPanelRenderer :
        public PanelRenderer {
public:
    explicit GLPanelRenderer(CommonGfx* gfx);

    void beginPanelRender(const PanelRenderContext& context) override;
    void endPanelRender() override;

    void drawCachedTexture(Texture* texture, const juce::Rectangle<float>& bounds) override;
    void drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
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
    void fillRect(float leftX, float topY, float rightX, float botY, bool scale) override;
    void fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
    void setClip(const juce::Rectangle<int>& clip) override;
    void setTransform(const juce::AffineTransform& transform) override;

private:
    Ref<CommonGfx> gfx;
};
