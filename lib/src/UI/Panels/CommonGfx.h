#pragma once

#include <vector>
#include "../../Array/Buffer.h"
#include "../../Array/BufferXY.h"
#include "../../Wireframe/Vertex/Vertex2.h"
#include "../../Obj/Ref.h"
#include "../../Obj/Color.h"
#include "../../Obj/ColorPos.h"

using std::vector;

class Panel;
class Texture;

class CommonGfx {
public:
    explicit CommonGfx(Panel* panel) : panel(panel) {}
    virtual ~CommonGfx() = default;

    virtual void drawPoint  (float size, Vertex2 point, bool scale)                 = 0;
    virtual void drawPoints (float size, BufferXY& xy, bool scale)                  = 0;
    virtual void drawPoints (float size, BufferXY& xy, Buffer<float> c, bool scale) = 0;
    virtual void drawLineStrip(BufferXY& xy, bool singleCall, bool scale)           = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2, bool scale)       = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) = 0;

    void drawLine(Vertex2 a, Vertex2 b, bool scale = true) {
        drawLine(a.x, a.y, b.x, b.y, scale);
    }

    void drawLine(Vertex2 a, Vertex2 b, const Color& c1, const Color& c2) {
        drawLine(a.x, a.y, b.x, b.y, c1, c2);
    }

    virtual void drawRect(float leftX, float topY, float rightX, float botY, bool scale)        = 0;
    virtual void fillRect(float leftX, float topY, float rightX, float botY, bool scale)        = 0;
    virtual void fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) = 0;

    virtual void fillAndOutlineColoured(const vector<ColorPos>& positions,
                                        float baseY, float baseAlpha,
                                        bool fill, bool outline) = 0;

    virtual void drawVerticalGradient(float left, float right, Buffer<float> y, const vector<Color>& colors) = 0;

    virtual void setCurrentColour(float, float, float, float)   = 0;
    virtual void setCurrentColour(float, float, float)          = 0;
    virtual void setCurrentColour(const Color& color)           = 0;
    virtual void setCurrentLineWidth(float width)               = 0;
    virtual void setOpacity(float opac) {}
    virtual void initRender() {}
    virtual void checkErrors() {}

    virtual void initializeTextures() = 0;
    virtual void disableSmoothing()                  = 0;
    virtual void drawTexture(Texture* tex)           = 0;
    virtual void enableSmoothing()                   = 0;
    virtual void updateTexture(Texture* tex)         = 0;
    virtual void drawSubTexture(Texture* tex, const Rectangle<float>& pos) = 0;

    void scaleIfNecessary(bool scale, BufferXY& xy);
    void scaleIfNecessary(bool scale, float& x1, float& y1);
    void scaleIfNecessary(bool scale, float& x1, float& y1, float& x2, float& y2);

    void drawFinalSelection();
    void drawBackground(const Rectangle<int>& bounds, bool fillBackground);

    Panel* getPanel() { return panel; }

protected:
    Ref<Panel> panel;
};