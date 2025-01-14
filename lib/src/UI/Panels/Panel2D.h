#pragma once
#include "Panel.h"
#include "../../Obj/Color.h"

class OpenGLPanel;
class BoundWrapper;

class Panel2D : public Panel {
public:
    Panel2D(SingletonRepo* repo, const String& name, bool curveIsBipolar, bool haveVertZoom);
    ~Panel2D() override;

    void init() override;

    void contractToRange(bool includeX = false);
    void drawCurvesFrom(BufferXY& buff, Buffer<float> alpha, const Color& colourA, const Color& colourB);
    static void prepareAlpha(const Buffer<float>& y, Buffer<float> alpha, float baseAlpha);
    void drawDeformerTags() override;
    void drawDepthLinesAndVerts() override;
    void highlightCurrentIntercept() override;
    void zoomUpdated(int updateSource) override;

    bool isCurveBipolar() const                              { return curveIsBipolar;            }
    void setCurveBipolar(bool bipolar)                       { curveIsBipolar = bipolar;         }
    void setColors(const Color& color1, const Color& color2) { colorA = color1; colorB = color2; }

    OpenGLPanel* getOpenglPanel()                            { return openGL.get(); }
    virtual bool shouldDrawCurve()                           { return true; }

    virtual bool isMeshEnabled();
    void drawCurvesAndSurfaces() override;
    void drawInterceptLines() override;

protected:
    bool curveIsBipolar, cyclicLines, haveVertZoom;

    Color colorA, colorB;

    std::unique_ptr<BoundWrapper> wrapper;
    std::unique_ptr<OpenGLPanel> openGL;
};
