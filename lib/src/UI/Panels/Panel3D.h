#pragma once
#include <UI/Layout/Dragger.h>

#include "Panel.h"

#include "../../UI/ColorGradient.h"
#include "../../Array/Column.h"

using std::vector;

class Interactor3D;
class OpenGLPanel3D;
class BoundWrapper;

class Panel3D :
        public Panel
    ,   public Dragger::Listener
{
public:
    class Renderer : public Panel::Renderer {
    public:
        void deactivate()   override = 0;
        void activate()     override = 0;
        void clear()        override = 0;

        virtual void drawBakedTextures()    {}
        virtual void gridDrawBeginning()    {}  // enable client arrays
        virtual void gridDrawFinished()     {}  // disable client arrays
        virtual void postVertsDraw()        {}
        virtual void scratchChannelSelected(int ch) {}
        virtual void textureBakeBeginning() {}
        virtual void textureBakeFinished()  {}

        virtual bool isDetailReduced()      { return false; }
        virtual bool isScratchApplicable()  { return false; }
        virtual bool shouldDrawGrid()       { return true;  }
        virtual bool willAdjustColumns()    { return false; }

        virtual Buffer<float> getColumnArray()      = 0;
        virtual const vector<Column>& getColumns()  = 0;
        virtual CriticalSection& getGridLock()      = 0;
        virtual void drawSurfaceColumn(int index)   = 0;
    };

    class DataRetriever {
    public:
        virtual Buffer<float> getColumnArray() = 0;
        virtual const vector<Column>& getColumns() = 0;
    };

    /* ----------------------------------------------------------------------------- */

    Panel3D(
        SingletonRepo* repo,
        const String& name,
        DataRetriever* dataRetriever,
        int updateSource,
        bool isTransparent,
        bool haveHorzZoom
    );

    ~Panel3D() override;
    void init() override;

    void dragStarted() override;
    void dragEnded() override;
    void zoomUpdated(int updateSource) override;
    void bakeTextures() override;
    void doExtraResized() override;
    void drawAxe();
    void drawPathTags() override;
    void drawDepthLinesAndVerts() override;
    void drawInterceptLines() override;
    void drawInterceptsAndHighlightClosest() override;
    void drawLinSurface(const vector<Column>& grid);
    void drawLogSurface(const vector<Column>& grid);
    void drawSurface();
    void freeResources();
    void highlightCurrentIntercept() override;

    virtual vector<Color>& getGradientColours();

    void drawCurvesAndSurfaces() override           { renderer->drawBakedTextures(); }
    void setGraphicsRenderer(Renderer* renderer)    { this->renderer.reset(renderer); }

    void setUseVertices(bool doso)          { useVertices = doso; }
    Renderer* getRenderer() const           { return renderer.get(); }
    OpenGLPanel3D* getOpenglPanel() const   { return openGL.get(); }

    void postVertsDraw() override;

protected:
    struct RenderParams {
        bool adjustColumns;
        bool reduced;

        int texHeight;
        int currKey, lastKey;
        int downsampSize;
        int sizeX, sizeY, lastSizeY, colSourceSizeY;
        int stride;

        Buffer<float> ramp;
    };

    void adjustLogColumnAndScale(int key, bool firstColumn);
    void doColourLookup32f      (Buffer<float> grd, Buffer<float> colors);
    void doColourLookup8u       (Buffer<Int8u> grd, Buffer<Int8u> colors);
    void doColumnDraw           (Buffer<Int8u> grd8u, Buffer<float> grd32f, int i);
    void downsampleColumn       (const Buffer<float>& column);
    void resampleColours        (Buffer<float> srcColors, Buffer<float> dstColors);
    void setVertices            (int column, Buffer<float> verts) const;
    void resizeArrays           ();
    void setColumnColourIndices ();

    /* ----------------------------------------------------------------------------- */

    static const int bytesPerFloat   = sizeof(float);
    static const int maxHorzLines    = 512;
    static const int vertsPerQuad    = 2;
    static const int coordsPerVert   = 2;
    static const int gradientWidth   = 512;

    bool useVertices, haveLogarithmicY, haveHorzZoom;
    int updateSource;
    float volumeScale, volumeTrans;

    DataRetriever* dataRetriever;
    Ref<Interactor3D> interactor3D;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<OpenGLPanel3D> openGL;
    std::unique_ptr<BoundWrapper> wrapper;

    ColorGradient gradient;
    RenderParams draw;

    ScopedAlloc<Int16s> clrIndicesA, clrIndicesB;
    ScopedAlloc<Float32> downsampAcc, downsampBuf, scaledX, scaledY;
    ScopedAlloc<Float32> fColours, fDestColours, vertices;
    ScopedAlloc<Int8u>  colours, texColours;

    /* ----------------------------------------------------------------------------- */

    friend class OpenGLPanel3D;

    JUCE_LEAK_DETECTOR(Panel3D);
};
