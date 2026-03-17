#pragma once

#include <iostream>
#include <vector>
#include "PanelRenderer.h"
#include "PanelDirtyState.h"
#include "PanelRenderContext.h"
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
class CommonGfx;
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
    PanelRenderContext createRenderContext() const;

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
    void repaint()                          { if (comp != nullptr) comp->repaint(); }

    bool isVisible() const                  { return comp != nullptr && comp->isVisible(); }
    int getNumCornersOverlapped() const     { return numCornersOverlapped;  }
    int getPanelId() const                  { return panelId;               }
    int getWidth() const                    { return comp != nullptr ? comp->getWidth() : 0; }
    int getHeight() const                   { return comp != nullptr ? comp->getHeight() : 0; }
    Rectangle<int> getBounds()              { return comp != nullptr ? comp->getBounds() : Rectangle<int>(); }

    Ref<Interactor> getInteractor()         { return interactor;            }
    const String& getName() override        { return panelName;             }
    CriticalSection& getRenderLock()        { return renderLock;            }
    bool isSpeedApplicable() const          { return speedApplicable;       }

    void setGraphicsHelper(CommonGfx* gfx);
    void setPanelRenderer(PanelRenderer* renderer) { panelRenderer = renderer; }
    void setRenderHelper(Renderer* util)    { renderHelper = util;          }
    void setSpeedApplicable(bool is)        { speedApplicable = is;         }
    void setNameTextureId(int id)           { currentNameId = id;           }
    void setNumCornersOverlapped(int num)   { numCornersOverlapped = num;   }
    void setComponent(Component* comp)      { this->comp = comp;            }

    void triggerPendingNameUpdate()         { pendingNameUpdate = true; dirtyState.mark(PanelDirtyState::Flag::StaticVisual); }
    void triggerPendingScaleUpdate()        { pendingScaleUpdate = true; dirtyState.mark(PanelDirtyState::Flag::StaticVisual); }
    void triggerPendingDeformUpdate()       { pendingDeformUpdate = true; dirtyState.mark(PanelDirtyState::Flag::Resource); }
    void markDirty(PanelDirtyState::Flag flag) { dirtyState.mark(flag);     }

    bool isDirty(PanelDirtyState::Flag flag) const { return dirtyState.isDirty(flag); }

    Component* getComponent()               { return comp;                  }
    PanelDirtyState& getDirtyState()        { return dirtyState;            }
    const PanelDirtyState& getDirtyState() const { return dirtyState;       }
    PanelRenderer* getPanelRenderer()       { return panelRenderer;         }

    void prepareBuffers(int size, int colorSize = -1) {
        xBuffer.ensureSize(size);
        yBuffer.ensureSize(size);

        xy.x = xBuffer.withSize(size);
        xy.y = yBuffer.withSize(size);

        if(colorSize > 0)
            cBuffer.ensureSize(colorSize);
    }

    template<class T>
    void prepareAndCopy(const vector<T>& vec) {
        int size = (int) vec.size();
        xBuffer.ensureSize(size);
        yBuffer.ensureSize(size);

        xy.x = xBuffer.withSize(size);
        xy.y = yBuffer.withSize(size);

        for(int i = 0; i < size; ++i)
            xy.set(i, vec[i]);
    }

    /* ----------------------------------------------------------------------------- */

    virtual void drawDepthLinesAndVerts() = 0;
    virtual void bakeTextures()                 {}
    virtual void drawCurvesAndSurfaces()        {}
    virtual void drawGuideCurveTags()             {}
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
    virtual void paintSharedCanvasBackground(juce::Graphics& g, const juce::Rectangle<int>& bounds) const;
    virtual void paintSharedCanvasSurface(juce::Graphics& g, const juce::Rectangle<int>& bounds) const;
    virtual void panelResized();
    virtual void updateBackground(bool onlyVerticalBackground = false);
    virtual bool usesSharedCanvasBackground() const { return false; }
    virtual bool usesSharedCanvasSurface() const { return false; }

    virtual bool isScratchApplicable()          { return false;             }
    virtual bool usesCachedSurface() const      { return false;             }
    virtual int getLayerScratchChannel()        { return CommonEnums::Null; }
    virtual void setInteractor(Interactor* itr) { this->interactor = itr;   }

protected:

    void handlePendingUpdates();
    void createGuideCurveTags();
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

    bool guideCurveApplicable;
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

    Texture *grabTex, *nameTexA, *guideCurveTex, *nameTexB, *scalesTex;
    Image scalesImage, guideCurveImage, grabImage, nameImage, nameImageB;

    BufferXY    xy;
    PanelDirtyState dirtyState;
    String      panelName;
    MicroTimer  renderTime, frameTime;
    Color       pointColours[5];
    CriticalSection renderLock;

    Ref<Component>  comp;
    Ref<Interactor> interactor;
    Ref<PanelRenderer> panelRenderer;
    Ref<Renderer>   renderHelper;
    std::unique_ptr<CommonGfx> gfx;

    Buffer<float> vertMajorLines, vertMinorLines, horzMajorLines, horzMinorLines;
    ScopedAlloc<float> xBuffer, yBuffer, cBuffer, stripRamp, spliceBuffer, bgLinesMemory;

    OwnedArray<Texture> textures;
    vector<Rectangle<float> > scales, guideCurveTags;

    /* ----------------------------------------------------------------------------- */

    friend class Interactor;
    friend class Interactor3D;
    friend class CommonGfx;
    friend class CommonGL;
    friend class CommonJ;

    JUCE_LEAK_DETECTOR(Panel);
};
