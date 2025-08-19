#pragma once

#include <vector>
#include <set>

#include "JuceHeader.h"
#include "Dimensions.h"
#include "MorphPositioner.h"
#include "PanelState.h"
#include "VertexTransformUndo.h"
#include "InteractorListener.h"

#include "../App/MeshLibrary.h"
#include "../App/SingletonAccessor.h"
#include "../Wireframe/CollisionDetector.h"
#include "../Wireframe/Vertex/Vertex2.h"
#include "../Wireframe/Interpolator/Trilinear/DepthVert.h"
#include "../Wireframe/Interpolator/Trilinear/TrilinearCube.h"
#include "../Wireframe/Interpolator/Trilinear/MorphPosition.h"
#include "../Design/Updating/Updateable.h"
#include "../Obj/Ref.h"
#include "../UI/AsyncUIUpdater.h"
#include "../UI/IConsole.h"

using std::vector;
using std::set;

class Panel;
class Vertex;
class OldMeshRasterizer;

typedef vector<Vertex2>::iterator CoordIter;
typedef vector<DepthVert>::iterator DepthIter;

class Interactor :
        public Updateable
    ,   public MouseListener
    ,   public virtual AsyncUIUpdater
    ,   public virtual SingletonAccessor {
public:

    class VertexProps {
    public:
        VertexProps() :
                ampVsPhaseApplicable(false)
            ,   isEnvelope(false) {
            dimensionNames.add("Time");
            dimensionNames.add("Phase");
            dimensionNames.add("Amp");
            dimensionNames.add("Red");
            dimensionNames.add("Blue");
            dimensionNames.add("Curve");

            for(int i = 0; i < numElementsInArray(sliderApplicable); ++i) {
                sliderApplicable[i] = true;
            }

            for(int i = 0; i < numElementsInArray(deformApplicable); ++i) {
                deformApplicable[i] = true;
            }
        }

        StringArray dimensionNames;

        bool sliderApplicable[Vertex::numElements]{};
        bool deformApplicable[Vertex::numElements]{};
        bool ampVsPhaseApplicable;
        bool isEnvelope;
    };

    /* ----------------------------------------------------------------------------- */

    Interactor(SingletonRepo* repo, const String& name, const Dimensions& d);
    ~Interactor() override = default;
    void init() override;

    virtual bool addNewCube(float startTime, float phase, float amp, float shape);
    void addNewCubeForMultipleIntercepts(TrilinearCube* addedLine, float startTime, float phase, float amp);
    void addNewCubeForOneIntercept(TrilinearCube* addedLine, float startTime, float phase, float amp, const MorphPosition& box);
    void addToArray(const Array<Vertex*>& src, vector<VertexFrame>& dst);
    void associateTo(Panel* panel);
    void clearSelectedAndCurrent();
    void clearSelectedAndRepaint();
    void copyVertexPositions();
    void deselectAll(bool forceDeselect = false);
    void doGlobalUIUpdate(bool force) override;
    void eraseSelected();
    void initVertsWithDepthDims(TrilinearCube* line, const vector<int>& depthDims, float x, float y, const MorphPosition& box);
    void postUpdateMessage();
    void reduceDetail() override;
    void restoreDetail() override;
    void refresh();
    void resetFinalSelection();
    void resetSelection();
    void resetState();
    void resizeFinalBoxSelection(bool recalculateFromVerts = true);
    void setAxeSize(float size);
    void setHighlitCorner(const MouseEvent& e, bool& wroteMessage);
    void setMouseDownStateSelectorTool(const MouseEvent& e);
    void setRasterizer(OldMeshRasterizer* rasterizer);
    void snapToGrid(Vertex2& toSnap);
    void translateVerts(vector<VertexFrame>& verts, const Vertex2& diff);
    void updateSelectionFrames();
    void validateLinePhases();

    void doMouseUpPencil(const MouseEvent& e);
    void doBoxSelect    (const MouseEvent& e);

    bool addNewCube(float startTime, float phase, float amp, float curveShape, const MorphPosition& cube);
    bool commitCubeAdditionIfValid(TrilinearCube*& addedLine,
        const vector<TrilinearCube*>& beforeLines,
        const vector<Vertex*>& beforeVerts);

    Array<Vertex*>      getVerticesToMove(TrilinearCube* cube, Vertex* startVertex);
    TrilinearCube*           getClosestLine(Vertex* vert);
    vector<Vertex*>&    getSelected();
    Vertex*             findClosestVertex(const Vertex2& posXY);

    const vector<VertexFrame>& getSelectedMovingVerts() const { return state.selectedFrame;     }
    CollisionDetector&  getCollisionDetector()                { return collisionDetector;   }
    CriticalSection&    getLock()                             { return vertexLock;          }
    OldMeshRasterizer*  getRasterizer() const                 { return rasterizer;          }
    MorphPosition       getOffsetPosition(bool withDepths)    { return positioner->getOffsetPosition(withDepths); }
    MorphPosition       getMorphPosition()                    { return positioner->getMorphPosition(); }

    float getYellow()   { return positioner->getYellow();   }
    float getRed()      { return positioner->getRed();      }
    float getBlue()     { return positioner->getBlue();     }

    /* ----------------------------------------------------------------------------- */

    virtual void transferLineProperties(TrilinearCube const* from, TrilinearCube* to1,  TrilinearCube* to2);

    void performUpdate(UpdateType updateType) override;
    virtual void updateDepthVerts();
    virtual void updateDspSync();
    virtual bool doesMeshChangeWarrantGlobalUpdate();
    virtual bool shouldDoDimensionCheck();
    virtual bool doCreateVertex();
    virtual float getDragMovementScale(TrilinearCube* cube);
    virtual MouseUsage getMouseUsage();
    virtual vector<TrilinearCube*> getLinesToSlideOnSingleSelect();

    void mouseDown      (const MouseEvent& e) override;
    void mouseUp        (const MouseEvent& e) override;
    void mouseDrag      (const MouseEvent& e) override;
    void mouseEnter     (const MouseEvent& e) override;
    void mouseExit      (const MouseEvent& e) override;
    void mouseMove      (const MouseEvent& e) override;
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel) override;
    virtual void commitPath         (const MouseEvent& e);

    virtual void doExtraMouseUp     () {}
    virtual void doZoomExtra        (bool commandDown);
    virtual void doClickSelect      (const MouseEvent& e);
    virtual void doDragCorner       (const MouseEvent& e);
    virtual void doDragMiddleZoom   (const MouseEvent& e);
    virtual void doDragPaintTool    (const MouseEvent& e);
    virtual void doDragVertex       (const MouseEvent& e);
    virtual void doExtraMouseDown   (const MouseEvent& e);
    virtual void doExtraMouseDrag   (const MouseEvent& e) {}
    virtual void doExtraMouseMove   (const MouseEvent& e);
    virtual void doReshapeCurve     (const MouseEvent& e);

    virtual bool locateClosestElement();
    virtual void moveSelectedVerts(const Vertex2& diff);
    virtual void showCoordinates();
    virtual void validateMesh();

    virtual void setMovingVertsFromSelected();
    virtual float getVertexClickProximityThres();
    virtual Mesh* getMesh();
    virtual Range<float> getVertexPhaseLimits(Vertex* vert);

    virtual void doFurtherReset()               {}
    virtual void focusGained()                  {}
    virtual void adjustAddedLine(TrilinearCube*)     {}
    virtual bool is3DInteractor()               { return false;         }
    virtual bool extraUpdateCondition()         { return true;          }
    virtual bool isCurrentMeshActive()          { return true;          }
    virtual bool isCursorApplicable(int tool)   { return true;          }
    virtual int  getUpdateSource()              { return updateSource;  }
    virtual Interactor* getOppositeInteractor() { return nullptr;       }

    /* ----------------------------------------------------------------------------- */

    bool vertsAreWaveApplicable;
    bool suspendUndo;

    int layerType;

    Dimensions dims;
    PanelState state;
    VertexProps vertexProps;

    Ref<Panel>      panel;
    Ref<Component>  display;

    Rectangle<int>  selection;
    Rectangle<int>  finalSelection;

    vector<Vertex2> pencilPath;
    vector<Vertex2> selectionCorners;
    vector<Vertex2> roundedCorners;
    vector<Vertex2> grips;
    vector<DepthVert> depthVerts;

    Range<float> vertexLimits[Vertex::numElements];

protected:

    void loopBacktrack          (vector<Vertex*>& loop, set<Vertex*>& alreadySeen, Vertex* current, int level);
    void updateLastValid        (vector<VertexFrame>& verts) const;
    void selectConnectedVerts   (set<Vertex*>& alreadySeen, Vertex* current);
    void addSelectedVertexOrToggle(Vertex* v);

    bool isDuplicateVertex(Vertex* v);

    Vertex* findLinesClosestVertex(TrilinearCube* cube, const Vertex2& mouseXY, Vertex& pos);
    Vertex* findLinesClosestVertex(TrilinearCube* cube, const Vertex2& mouseXY);
    Vertex* findLinesClosestVertex(TrilinearCube* cube, const Vertex2& mouseXY, const MorphPosition& pos);

    MorphPosition getModPosition(bool adjust = true);
    MorphPosition getCube();

    virtual Vertex* findClosestVertex();

    /* ----------------------------------------------------------------------------- */

    bool ignoresTime, scratchesTime;
    bool exitBacktrackEarly;

    int updateSource;

    MorphPositioner*    positioner;
    OldMeshRasterizer*     rasterizer;

    CriticalSection     vertexLock;
    CollisionDetector   collisionDetector;
    VertexTransformUndo vertexTransformUndoer;
    TrilinearCube::ReductionData reduceData;

    ListenerList<InteractorListener> listeners;

    JUCE_LEAK_DETECTOR(Interactor)
};