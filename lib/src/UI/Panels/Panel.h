#pragma once

#include <iostream>
#include <vector>
#include "CommonGfx.h"
#include "ZoomPanel.h"
#include "JuceHeader.h"
#include "../../App/SingletonAccessor.h"
#include "../../Array/BufferXY.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Obj/Color.h"
#include "../../Obj/Ref.h"
#include "../../Util/Arithmetic.h"
#include "../../Util/CommonEnums.h"
#include "../../Util/MicroTimer.h"

using std::vector;

class Interactor;
class Interactor3D;
class Texture;

class Panel :
        public virtual SingletonAccessor
    ,   public ZoomPanel::ZoomListener {
public:
    static constexpr int linestripRes = 256;

    enum { maxMajorSize = 64, maxMinorSize = 256 };
    enum { GrabTexture, NameTexture, NameTextureB, BackTexture, ScalesTexture, numTextures  };

    class Renderer {
    public:
        virtual ~Renderer() = default;

        virtual void deactivate()   = 0;
        virtual void activate()     = 0;
        virtual void clear()        = 0;
    };

    /* ----------------------------------------------------------------------------- */

    Panel(SingletonRepo* repo, const String& name, bool isTransparent = true);
    ~Panel() override;

    void bakeTexturesNextRepaint();
    void drawFinalSelection();
    void drawOutline();
    void drawPencilPath();
    void drawScaledInterceptPoints(int size);
    void drawSelectionRectangle();
    void drawViewableVerts();
    void highlightSelectedVerts();
    void render();
    void setCursor();
    void triggerZoom(bool in);
    void updateNameTexturePos();
    void updateVertexSizes();

    void applyScale         (BufferXY& buff);
    void applyScaleX        (Buffer<float> array);
    void applyScaleY        (Buffer<float> array);
    void applyNoZoomScaleX  (Buffer<float> array);
    void applyNoZoomScaleY  (Buffer<float> array);

    void doZoomExtra(bool commandDown) override;
    void zoomUpdated(int updateSource) override;

    bool createLinePath(const Vertex2& start, const Vertex2& end, VertCube* cube, int pointDim, bool haveSpeed);
    void createNameImage(const String& displayName, bool isSecondImage = false, bool dark = false);

    /* ----------------------------------------------------------------------------- */

    float sx(float x)   const;
    float sxnz(float x) const;
    float invertScaleX(int x)   const;
    float invertScaleXNoZoom(int x)     const;
    float sy(float y)   const;
    float synz(float y) const;
    float invertScaleY(int y)   const;
    float invertScaleYNoZoom(int y)     const;
    float invScaleXNoDisp(int x) const;
    float invScaleYNoDisp(int y) const;

    void clear()                            { renderHelper->clear();        }
    void deactivateContext()                { renderHelper->deactivate();   }
    void activateContext()                  { renderHelper->activate();     }
    void repaint()                          { comp->repaint();              }

    bool isVisible() const                  { return comp->isVisible();     }
    int getNumCornersOverlapped() const     { return numCornersOverlapped;  }
    int getWidth() const                    { return comp->getWidth();      }
    int getHeight() const                   { return comp->getHeight();     }
    Rectangle<int> getBounds()              { return comp->getBounds();     }

    Ref<Interactor> getInteractor()         { return interactor;            }
    const String& getName() override        { return panelName;             }
    CriticalSection& getRenderLock()        { return renderLock;            }
    bool isSpeedApplicable() const          { return speedApplicable;       }

    void setGraphicsHelper(CommonGfx* gfx)  { this->gfx.reset(gfx);         }
    void setRenderHelper(Renderer* util)    { renderHelper = util;          }
    void setSpeedApplicable(bool is)        { speedApplicable = is;         }
    void setNameTextureId(int id)           { currentNameId = id;           }
    void setNumCornersOverlapped(int num)   { numCornersOverlapped = num;   }
    void setComponent(Component* comp)      { this->comp = comp;            }

    void triggerPendingScaleUpdate()        { pendingScaleUpdate = true;    }
    void triggerPendingDeformUpdate()       { pendingDeformUpdate = true;   }

    Component* getComponent()               { return comp;                  }

    void prepareBuffers(int size, int colorSize = -1) {
        xBuffer.ensureSize(size);
        yBuffer.ensureSize(size);

        xy.x = xBuffer.withSize(size);
        xy.y = yBuffer.withSize(size);

        if (colorSize > 0) {
            cBuffer.ensureSize(colorSize);
}
    }

    template<class T>
    void prepareAndCopy(const vector<T>& vec) {
        int size = (int) vec.size();
        xBuffer.ensureSize(size);
        yBuffer.ensureSize(size);

        xy.x = xBuffer.withSize(size);
        xy.y = yBuffer.withSize(size);

        for (int i = 0; i < size; ++i) {
            xy.set(i, vec[i]);
}
    }

    /* ----------------------------------------------------------------------------- */

    virtual void drawDepthLinesAndVerts() = 0;
    virtual void bakeTextures()                 {}
    virtual void drawCurvesAndSurfaces()        {}
    virtual void drawDeformerTags()             {}
    virtual void drawInterceptLines()           {}
    virtual void drawVerticalLine()             {}
    virtual void highlightCurrentIntercept()    {}
    virtual void doExtraResized()               {}
    virtual void drawScales()                   {}
    virtual void postCurveDraw()                {}
    virtual void postVertsDraw()                {}
    virtual void preDraw()                      {}

    virtual void componentChanged();
    void constrainZoom() override;
    virtual void drawBackground(bool fillBackground = true);
    virtual void drawBackground(const Rectangle<int>& bounds, bool fillBackground);
    virtual void drawInterceptsAndHighlightClosest();
    virtual void panelResized();
    virtual void updateBackground(bool onlyVerticalBackground = false);

    virtual bool isScratchApplicable()          { return false;             }
    virtual int getLayerScratchChannel()        { return CommonEnums::Null; }
    virtual void setInteractor(Interactor* itr) { this->interactor = itr;   }

protected:

    void handlePendingUpdates();
    void createDeformerTags();
    virtual void createScales() {}

    /* ----------------------------------------------------------------------------- */

    bool alwaysDrawDepthLines;
    bool backgroundTempoSynced;
    bool backgroundTimeRelevant;
    bool doesDrawMouseHint;
    bool drawLinesAfterFill;
    bool isTransparent;
    bool pendingDeformUpdate;
    bool pendingNameUpdate;
    bool pendingScaleUpdate;
    bool shouldBakeTextures;

    bool deformApplicable;
    bool speedApplicable;

    int currentNameId;
    int numCornersOverlapped;
    int paddingLeft;
    int paddingRight;
    int panelId;
    int vertPadding;

    int64 lastFrameTime;

    float maxWidth;
    float minorBrightness, majorBrightness;
    float bgPaddingLeft, bgPaddingRight, bgPaddingTop, bgPaddingBttm;
    float vertexBlackRadius, vertexWhiteRadius, vertexHighlightRadius, vertexSelectedRadius;

    double averageFrameTime;

    Point<float> nameCornerPos;

    Texture *grabTex, *nameTexA, *dfrmTex, *nameTexB, *scalesTex;
    Image scalesImage, dfrmImage, grabImage, nameImage, nameImageB;

    BufferXY    xy;
    String      panelName;
    MicroTimer  renderTime, frameTime;
    Color       pointColours[5];
    CriticalSection renderLock;

    Ref<Component>  comp;
    Ref<Interactor> interactor;
    Ref<Renderer>   renderHelper;
    std::unique_ptr<CommonGfx> gfx;

    Buffer<float> vertMajorLines, vertMinorLines, horzMajorLines, horzMinorLines;
    ScopedAlloc<float> xBuffer, yBuffer, cBuffer, stripRamp, spliceBuffer, bgLinesMemory;

    OwnedArray<Texture> textures;
    vector<Rectangle<float> > scales, dfrmTags;

    /* ----------------------------------------------------------------------------- */

    friend class Interactor;
    friend class Interactor3D;
    friend class CommonGfx;
    friend class CommonGL;
    friend class CommonJ;

    JUCE_LEAK_DETECTOR(Panel);
};