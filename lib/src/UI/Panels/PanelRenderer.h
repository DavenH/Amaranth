#pragma once

#include <vector>

#include "JuceHeader.h"
#include "../../Array/Buffer.h"
#include "../../Array/BufferXY.h"
#include "../../Curve/Vertex2.h"
#include "../../Obj/Color.h"
#include "../../Obj/ColorPos.h"

class PanelRenderContext;
class Texture;

class PanelRenderer {
public:
    virtual ~PanelRenderer() = default;

    virtual void beginPanelRender(const PanelRenderContext& context) = 0;
    virtual void endPanelRender() = 0;

    virtual void drawCachedTexture(Texture* texture, const juce::Rectangle<float>& bounds) = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) = 0;
    virtual void drawLineStrip(BufferXY& xy, bool scale) = 0;
    virtual void drawPoints(float size, BufferXY& xy, bool scale) = 0;
    virtual void drawPoints(float size, BufferXY& xy, Buffer<float> c, bool scale) = 0;
    virtual void drawRect(float leftX, float topY, float rightX, float botY, bool scale) = 0;
    virtual void fillAndOutlineColoured(
        const std::vector<ColorPos>& positions,
        float baseY,
        float baseAlpha,
        bool fill,
        bool outline
    ) = 0;
    virtual void fillRect(float leftX, float topY, float rightX, float botY, bool scale) = 0;
    virtual void fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) = 0;
    virtual void setClip(const juce::Rectangle<int>& clip) = 0;
    virtual void setTransform(const juce::AffineTransform& transform) = 0;
};
