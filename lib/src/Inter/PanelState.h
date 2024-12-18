#pragma once

#include <vector>
#include "../Curve/Vertex.h"
#include "../Curve/Vertex2.h"

using std::vector;

class VertexFrame {
public:
    VertexFrame(Vertex* vert, const Vertex2& o) : vert(vert), origin(o), lastValid(o) {}

    Vertex* vert;
    Vertex2 origin, lastValid;
};

class CoordFrame {
    Vertex2 origin, lastValid;
    Vertex2 pos;
};

typedef vector<VertexFrame>::iterator FrameIter;

class PanelState {
private:
public:
    virtual ~PanelState() = default;

    vector<VertexFrame> singleHorz, singleXY, singleAll;
    vector<VertexFrame> selectedFrame;

    vector<Vertex> positions;

//	vector<CoordFrame> selectionCorners;
    vector<Vertex2> cornersStart;
    vector<Vertex2> pivots;

    enum Flags {
        DidMeshChange,
        DidIncrementalMeshChange,

        LoweredRes,
        SimpleRepaint,

        numFlags
    };

    enum MouseFlags {
        FirstMove,
        MouseOver,
        WithinReshapeThresh,

        numMouseFlags
    };

    enum PanelValues {
        HighlitCorner,
        CurrentCurve,
        CurrentPivot,

        numValues
    };

    enum RealValues {
        PencilSmooth,
        PencilRadius,

        numRealValues
    };

    enum ActionState {
        Idle,

        ClickSelecting,
        DraggingVertex,
        CreatingVertex,
        ReshapingCurve,
        BoxSelecting,
        DraggingCorner,
        SelectingConnected,

        Rotating,
        Extruding,
        Copying,
        Cutting,
        Stretching,

        PaintingCreate,
        PaintingEdit,
        Nudging,

        MiddleZooming,

        // keep these mouse-related states grouped
        // these, we wish to carry over from the mousemove event to the mousedown
        numPanelStates
    };

    enum Position {
        LeftPivot,
        TopPivot,
        RightPivot,
        BottomPivot,
        CentrePivot,

        numSelectionPositions
    };

    enum Corners {
        Left, Top, Right, Bot, MoveHandle, RotateHandle
    };

    enum CornerTypes {
        LeftRightCorner, DiagCorner, TopBottomCorner,
    };

    bool flags[numFlags]{};
    bool mouseFlags[numMouseFlags]{};
    int values[numValues]{};
    float realValues[numRealValues]{};

    ActionState actionState;
//	char states[numPanelStates];

    VertCube* currentCube;
    Vertex* currentVertex;

    // 3d-specific
    int currentIcpt{};
    int currentFreeVert{};

    // mouse position
    Vertex2 start;
    Vertex2 currentMouse;
    Vertex2 lastMouse;

    PanelState() {
        for(int i = 0; i < numSelectionPositions; ++i) {
            pivots.emplace_back();
        }

        currentVertex = nullptr;
        currentCube = nullptr;

        realValues[PencilSmooth] = 0.2f;
        realValues[PencilRadius] = 0.04f;

        PanelState::reset();
    }

    virtual void reset() {
        resetFlags();
        resetMouseFlags();
        resetActionState();
        resetValues();

        currentVertex 	= nullptr;
        currentCube 	= nullptr;
    }

    void resetValues() {
        values[CurrentPivot] = CentrePivot;
        values[HighlitCorner] = -1;
    }

    void resetFlags() {
        for(bool& i : flags) {
            i = false;
        }
    }

    void resetMouseFlags() {
        for (bool& i : mouseFlags) {
            i = false;
        }

        mouseFlags[FirstMove] = true;
    }

    void resetActionState() {
        actionState = Idle;
    }

    Vertex2 getClosestPivot() {
        return pivots[values[CurrentPivot]];
    }

    JUCE_LEAK_DETECTOR(PanelState);
};