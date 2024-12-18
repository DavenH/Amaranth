#pragma once

#include "CommonGfx.h"

class Panel;
class OpenGLBase;

class CommonGL :
        public CommonGfx {
public:
    CommonGL(Panel* panel, OpenGLBase* parent);
    ~CommonGL() override = default;

    void checkErrors() override;
    void disableSmoothing() override;
    void drawLine(float x1, float y1, float x2, float y2, bool scale) override;
    void drawLine(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
    void drawLineStrip(BufferXY& xy, bool singleCall, bool scale) override;
    void drawPoint(float size, Vertex2 point, bool scale) override;
    void drawPoints(float size, BufferXY& xy, bool scale) override;
    void drawPoints(float size, BufferXY& xy, Buffer<float> c, bool scale) override;
    void drawRect(float leftX, float topY, float rightX, float botY, bool scale) override;
    void drawSubTexture(Texture* tex, const Rectangle<float>& pos) override;
    void drawTexture(Texture* tex) override;
    void drawVerticalGradient(float left, float right, Buffer<float> y, const vector<Color>& colors) override;
    void enableSmoothing() override;
    void fillAndOutlineColoured(const vector<ColorPos>& positions, float baseY, float baseAlpha, bool fill, bool outline) override;
    void fillRect(float leftX, float topY, float rightX, float botY, bool scale) override;
    void fillRect(float x1, float y1, float x2, float y2, const Color& c1, const Color& c2) override;
    void initializeTextures() override;
    void initLineParams();
    void initRender() override;
    void setCurrentColour(const Color& color) override;
    void setCurrentColour(float, float, float, float) override;
    void setCurrentColour(float, float, float) override;
    void setCurrentLineWidth(float width) override;
    void updateTexture(Texture* tex) override;

private:
    Ref<OpenGLBase> parent;
};
