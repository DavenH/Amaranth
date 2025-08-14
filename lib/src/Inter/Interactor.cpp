#include <iterator>
#include <App/AppConstants.h>

#include "../App/DocumentLibrary.h"
#include "../App/EditWatcher.h"
#include "../App/MeshLibrary.h"
#include "../App/Settings.h"
#include "../App/SingletonRepo.h"
#include "../Definitions.h"
#include "../Design/Updating/Updater.h"
#include "../Obj/CurveLine.h"
#include "../UI/Panels/Panel.h"
#include "../UI/Panels/Texture.h"
#include "../UI/Panels/ZoomRect.h"
#include "../Util/CommonEnums.h"
#include "../Util/NumberUtils.h"
#include "../Util/Util.h"
#include "../Wireframe/Interpolator/Trilinear/TrilinearVertex.h"
#include "../Wireframe/OldMeshRasterizer.h"
#include "Interactor.h"
#include "Interactor3D.h"
#include "UndoableActions.h"
#include "UndoableMeshProcess.h"

Interactor::Interactor(SingletonRepo* repo, const String& name, const Dimensions& d) :
        SingletonAccessor       (repo, name)
    ,   dims                    (d)
    ,   vertsAreWaveApplicable  (false)
    ,   suspendUndo             (false)
    ,   ignoresTime             (false)
    ,   scratchesTime           (false)
    ,   vertexTransformUndoer   (this)
    ,   updateSource            (CommonEnums::Null)
    ,   layerType               (CommonEnums::Null)
    ,   collisionDetector       (repo, CollisionDetector::Time) {
    exitBacktrackEarly = false;

    for(int i = 0; i < numElementsInArray(vertexLimits); ++i) {
        vertexLimits[i] = Range(0.f, 1.f);
    }
}

bool Interactor::isDuplicateVertex(Vertex* v) {
    vector<Vertex*>& selected = getSelected();
    return std::any_of(selected.begin(), selected.end(),
                      [v](const Vertex* vertex) { return vertex == v; });
}

void Interactor::addSelectedVertexOrToggle(Vertex* v) {
    if (v == nullptr) {
        return;
    }

    bool duplicate = false;

    vector<Vertex*>& selected = getSelected();
    int oldSize = (int) selected.size();

    for (auto it = selected.begin(); it != selected.end(); ++it) {
        if (v == *it) {
            selected.erase(it);
            duplicate = true;
            break;
        }
    }

    if (!duplicate) {
        selected.push_back(v);
        state.currentVertex = v;
    }

    if (oldSize != static_cast<int>(selected.size())) {
        flag(SimpleRepaint) = true;

        resizeFinalBoxSelection();
        setMovingVertsFromSelected();
    }
}

void Interactor::clearSelectedAndRepaint() {
    ScopedLock sl(vertexLock);

    clearSelectedAndCurrent();

    state.currentCube = nullptr;
    state.currentVertex = nullptr;
}

void Interactor::mouseMove(const MouseEvent& e) {
    mouseFlag(MouseOver) = true;
    flag(DidMeshChange) = false;

    state.lastMouse = state.currentMouse;
    state.currentMouse = Vertex2(panel->invertScaleX(e.x), panel->invertScaleY(e.y));

    NumberUtils::constrain<float>(state.currentMouse.x, vertexLimits[dims.x]);

    bool changedVertex = locateClosestElement();
    if(changedVertex && !getSetting(ViewVertsOnlyOnHover)) {
        if (Interactor * itr = getOppositeInteractor()) {
            if (is3DInteractor())
                itr->state.currentMouse.x = state.currentMouse.y;
            else {
                itr->state.currentMouse.x = positioner->getValue(positioner->getPrimaryDimension());
                itr->state.currentMouse.y = state.currentMouse.x;
            }

            itr->locateClosestElement();
            itr->display->repaint();
        }
    }

    bool wroteMessage = false;

    setHighlitCorner(e, wroteMessage);
    doExtraMouseMove(e);

    if(! wroteMessage) {
        showCoordinates();
    }

    panel->setCursor();
    flag(SimpleRepaint) |= getSetting(Tool) == Tools::Axe || actionIs(Cutting);

    refresh();
}

void Interactor::mouseDown(const MouseEvent& e) {
    state.resetActionState();
    PanelState::ActionState& action = state.actionState;

    state.currentMouse = Vertex2(panel->invertScaleX(e.x), panel->invertScaleY(e.y));
    NumberUtils::constrain<float>(state.currentMouse.x, vertexLimits[dims.x]);
    state.start = state.currentMouse;

    bool selectWithRight = getSetting(SelectWithRight) == 1;

    updateSelectionFrames();

    if(getSetting(Tool) == Tools::Selector) {
        setMouseDownStateSelectorTool(e);
    } else if (getSetting(Tool) == Tools::Pencil) {
        bool leftIsDown     = e.mods.isLeftButtonDown();
        bool rightIsDown     = e.mods.isRightButtonDown();

        action = (leftIsDown && selectWithRight   || rightIsDown && ! selectWithRight)     ? PanelState::PaintingCreate :
                 (leftIsDown && ! selectWithRight || rightIsDown && selectWithRight)     ? PanelState::PaintingEdit : PanelState::Idle;

        if (actionIs(PaintingEdit)) {
            vertexTransformUndoer.start();
        }
    }

    else if(getSetting(Tool) == Tools::Nudge) {
        action = PanelState::Nudging;
    }

    bool createSucceeded = false;

    if(actionIs(CreatingVertex)) {
        createSucceeded = doCreateVertex();
    }

    if (actionIs(ClickSelecting)) {
        doClickSelect(e);
    }

    panel->setCursor();
    doExtraMouseDown(e);

    listeners.call(&InteractorListener::focusGained, this);

    refresh();

    // rasterizer should have added new point if applicable
    locateClosestElement();
}


void Interactor::mouseUp(const MouseEvent& e) {
    if (actionIs(BoxSelecting) || actionIs(ClickSelecting) || actionIs(DraggingCorner)) {
        vector<Vertex*>& selected = getSelected();
        if (!selected.empty()) {
            setMovingVertsFromSelected();

            listeners.call(&InteractorListener::focusGained, this);

            resizeFinalBoxSelection();
        }

        flag(SimpleRepaint) = true;
    }

    if(getSetting(Tool) == Tools::Pencil) {
        doMouseUpPencil(e);
    }

    vertexTransformUndoer.commitIfPending();
    selection.setBounds(0, 0, 0, 0);

    doExtraMouseUp();

    mouseFlag(FirstMove) = true;

    if (flag(DidMeshChange) && doesMeshChangeWarrantGlobalUpdate()) {
        triggerRestoreUpdate();
    }

    refresh();

    flag(LoweredRes) = false;
    flag(SimpleRepaint) = false;
}

void Interactor::mouseDrag(const MouseEvent& e) {
    state.lastMouse = state.currentMouse;
    state.currentMouse     = Vertex2(panel->invertScaleX(e.x), panel->invertScaleY(e.y));

    NumberUtils::constrain<float>(state.currentMouse.x, vertexLimits[dims.x]);

    bool selectWithRight = getSetting(SelectWithRight) == 1;

    bool isSelecting =
            ! actionIs(DraggingCorner) && getSetting(Tool) == Tools::Selector &&
            (selectWithRight && e.mods.isRightButtonDown() || (! selectWithRight && e.mods.isLeftButtonDown()) &&
                    state.actionState != PanelState::ReshapingCurve);

    if(actionIs(CreatingVertex) || isSelecting) {
        state.actionState = PanelState::DraggingVertex;

        if (!isDuplicateVertex(state.currentVertex) && state.currentVertex != nullptr) {
            getSelected().push_back(state.currentVertex);
            updateSelectionFrames();
        }
    }

    switch (state.actionState) {
//        case PanelState::Nudging:      doNudge(e);          break;
        case PanelState::MiddleZooming:  doDragMiddleZoom(e); break;
        case PanelState::BoxSelecting:   doBoxSelect(e);      break;
        case PanelState::DraggingCorner: doDragCorner(e);     break;
        case PanelState::PaintingCreate:
        case PanelState::PaintingEdit:   doDragPaintTool(e);  break;
        case PanelState::DraggingVertex: doDragVertex(e);     break;
        case PanelState::ReshapingCurve: doReshapeCurve(e);   break;
        default: break;
    }

    doExtraMouseDrag(e);

    if (flag(LoweredRes) && mouseFlag(FirstMove) && getSetting(UpdateGfxRealtime) &&
        doesMeshChangeWarrantGlobalUpdate()) {
        triggerReduceUpdate();
    }

    showCoordinates();
    refresh();

    mouseFlag(FirstMove) = false;
}

void Interactor::mouseEnter(const MouseEvent& e) {
    mouseFlag(MouseOver) = true;

    String keys = is3DInteractor() ? "a, c, e, del" : "a, del";
    repo->getConsole().setMouseUsage(getMouseUsage());
    repo->getConsole().setKeys(keys);

    focusGained();
    display->repaint();
}

void Interactor::mouseExit(const MouseEvent& e) {
    state.resetMouseFlags();

    display->repaint();
}

void Interactor::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) {
    float yInc = wheel.deltaY;
    int tool = getSetting(Tool);

    if (e.mods.isShiftDown()) {
        ZoomRect& rect = panel->zoomPanel->rect;

        rect.x += rect.w * 0.05 * (yInc > 0 ? 1 : -1);
        panel->constrainZoom();

        return;
    }

    if (tool == Tools::Axe) {
        realValue(PencilRadius) *= yInc < 0 ? 1.5f : 0.667f;

        showConsoleMsg(String("Axe size: ") + String(realValue(PencilRadius), 1));
    } else {
        state.currentMouse = Vertex2(-1, 0);

        if(yInc > 0) {
            panel->zoomPanel->zoomIn(e.mods.isCommandDown(), e.x, e.y);
        }

        if(yInc < 0) {
            panel->zoomPanel->zoomOut(e.mods.isCommandDown(), e.x, e.y);
        }
    }
}

void Interactor::copyVertexPositions() {
    if (Mesh* mesh = getMesh()) {
        state.positions.clear();

        for(auto vert : mesh->getVerts())
            state.positions.push_back(*vert);
    }
}

void Interactor::addToArray(const Array<Vertex*>& src, vector<VertexFrame>& dst) {
    for(auto vert : src) {
        Vertex2 origin(vert->values[dims.x], vert->values[dims.y]);

        dst.emplace_back(vert, origin);
    }
}

void Interactor::updateSelectionFrames() {
    copyVertexPositions();

    state.singleHorz    .clear();
    state.singleXY        .clear();
    state.singleAll        .clear();

    MorphPosition morph = getModPosition();

    {
        vector<TrilinearCube*> linesToSlide = getLinesToSlideOnSingleSelect();

        for (auto& it : linesToSlide) {
            Array<Vertex*> arr = getVerticesToMove(it, it->findClosestVertex(morph));

            addToArray(arr, state.singleHorz);
        }

        addToArray(getVerticesToMove(state.currentCube, state.currentVertex), state.singleXY);

        std::copy(state.singleXY.begin(),   state.singleXY.end(),   std::back_inserter(state.singleAll));
        std::copy(state.singleHorz.begin(), state.singleHorz.end(), std::back_inserter(state.singleAll));
    }

    setMovingVertsFromSelected();

    state.cornersStart.clear();

    for (auto& selectionCorner : selectionCorners) {
        state.cornersStart.push_back(selectionCorner);
    }

    float x, y;
    float leftX = 100,     centreX = 0, rightX = 0;
    float topY     = 0,     centreY = 0, lowY     = 1;
    bool wrapsPhase     = getRasterizer()->wrapsVertices();
    bool ignoresTime    = vertexProps.isEnvelope;
    int numGood         = 0;

    vector<Vertex*>& selected = getSelected();

    for (auto vert : selected) {
        bool timePole = false, red = false, blue = false;

        if (vert == nullptr) {
            jassertfalse;
            continue;
        }

        TrilinearCube* closestLine = getClosestLine(vert);

        if(closestLine != nullptr)
            closestLine->getPoles(vert, timePole, red, blue);

        x = vert->values[dims.x];
        y = vert->values[dims.y];

        if(x >= 1 && wrapsPhase) {
            x -= 1;
        }

        if (x < leftX)    leftX  = x;
        if (x > rightX)    rightX = x;
        if (y < lowY)    lowY   = y;
        if (y > topY)    topY   = y;

        if(! (ignoresTime && timePole)) {
            centreX += x;
            centreY += y;

            ++numGood;
        }
    }

    centreX *= 1 / float(numGood);
    centreY *= 1 / float(numGood);

    state.pivots[PanelState::TopPivot]         = Vertex2(centreX,     topY);
    state.pivots[PanelState::BottomPivot]     = Vertex2(centreX,     lowY);
    state.pivots[PanelState::LeftPivot]     = Vertex2(leftX,     centreY);
    state.pivots[PanelState::RightPivot]     = Vertex2(rightX,     centreY);
    state.pivots[PanelState::CentrePivot]     = Vertex2(centreX,     centreY);

    listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
}

void Interactor::deselectAll(bool forceDeselect) {
    vector<Vertex*>& selected = getSelected();
    selected.clear();

    resetFinalSelection();
    updateSelectionFrames();

    display->repaint();
}

void Interactor::eraseSelected() {
    vector<Vertex*>& selected = getSelected();

    if(selected.empty()) {
        return;
    }

    Mesh* mesh = getMesh();

    if(mesh == nullptr) {
        return;
    }

    UndoableMeshProcess eraseProcess(this, "Erase selected");

    if (dims.numHidden() > 0) {
        set<TrilinearCube*> linesToDelete;

        for (auto& it : selected) {
            for (auto& owner : it->owners)
                linesToDelete.insert(owner);
        }

        for (auto cube : linesToDelete) {
            if (mesh->removeCube(cube)) {
                for (int i = 0; i < TrilinearCube::numVerts; ++i) {
                    Vertex* vert = cube->getVertex(i);

                    if (vert->getNumOwners() == 1) {
                        mesh->removeVert(vert);

                        // remove now so that only free vert are left
                        selected.erase(std::find(selected.begin(), selected.end(), vert));
                    } else {
                        vert->removeOwner(cube);
                    }
                }
            } else {
                cout << "tried to erase line but it wasn't in array" << "\n";
            }

            // warning:
            // don't delete line from memory yet because undo might reclaim it?
        }

        mesh->validate();

        // remove selected free verts
        for (auto& it : selected) {
            mesh->removeVert(it);
        }

        selected.clear();
    } else {
        for (auto& it : selected) {
            mesh->removeVert(it);
        }

        selected.clear();
    }

    // update vertex vector action should do this
    // what if I'm dragging a vert and then I click 'x'?
    //    selected.clear();

    updateSelectionFrames();
    resetFinalSelection();

    flag(DidMeshChange) = true;
}

void Interactor::associateTo(Panel* panel) {
    this->panel = panel;
    this->display = panel->comp;

    display->addMouseListener(this, false);
    panel->setInteractor(this);
}

void Interactor::init() {
    positioner = &repo->getMorphPosition();
}

Vertex* Interactor::findClosestVertex() {
    return findClosestVertex(state.currentMouse);
}

Vertex* Interactor::findClosestVertex(const Vertex2& posXY) {
    MorphPosition morphPos = getModPosition();
    Vertex pos;

    // envelope panel will specialize this to disregard 'time' value
    pos[Vertex::Amp  ]     = -1;
    pos[Vertex::Phase]     = -1;
    pos[Vertex::Time ]     = morphPos.time;
    pos[Vertex::Red  ]     = morphPos.red;
    pos[Vertex::Blue ]     = morphPos.blue;

    float minDist = 1000;
    float dist;
    pos[dims.x] = posXY.x;

    // don't want to factor in amplitude position in 2D selection
    if (dims.y != Vertex::Amp) {
        pos[dims.y] = posXY.y;
    }

    Vertex* closest = 0;
    Mesh* mesh = getMesh();

    if (mesh->getNumVerts() == 0) {
        return nullptr;
    }

    float diff;
    for (auto vert : mesh->getVerts()) {
        dist = 0;

        for (int i = 0; i < 5; ++i) {
            if (pos[i] < 0)
                continue;

            float val = vert->values[i];

            diff = pos[i] - val;
            dist += diff * diff;
        }

        if (dist < minDist) {
            minDist = dist;
            closest = vert;
        }
    }

    return closest;
}

void Interactor::commitPath(const MouseEvent& e) {
    if (pencilPath.empty()) {
        return;
    }

    if (actionIs(PaintingCreate)) {
        MorphPosition morphPos = getModPosition();

        ScopedLock sl(vertexLock);

        if (Mesh* mesh = getMesh()) {
            UndoableMeshProcess commitProcess(this, "Pencil draw");

            for(auto& v : pencilPath) {
                auto* vert = new Vertex();

                vert->values[Vertex::Time] = morphPos.time;
                vert->values[Vertex::Red ] = morphPos.red;
                vert->values[Vertex::Blue] = morphPos.blue;
                vert->values[Vertex::Amp]  = 0.5f;

                vert->values[dims.x]       = v.x;    //will overwrite whichever axis we're on
                vert->values[dims.y]       = v.y;

                mesh->addVertex(vert);
            }
        }

        flag(DidMeshChange) = true;
    }
}

void Interactor::doExtraMouseMove(const MouseEvent& e) {
}

void Interactor::doExtraMouseDown(const MouseEvent& e) {
}

bool Interactor::locateClosestElement() {
    ScopedLock sl(vertexLock);

    Vertex* lastCurrent = state.currentVertex;

    return Util::assignAndWereDifferent(state.currentVertex, findClosestVertex());
}

void Interactor::doZoomExtra(bool commandDown) {
    resizeFinalBoxSelection();
}

void Interactor::resetState() {
    ScopedLock sl(vertexLock);

    state.reset();
    depthVerts.clear();
    getSelected().clear();
    state.selectedFrame.clear();

    resetFinalSelection();
    doFurtherReset();
}

void Interactor::showCoordinates() {
    showConsoleMsg(String::formatted("%3.2f, %3.2f", state.currentMouse.x, state.currentMouse.y));
}

void Interactor::resizeFinalBoxSelection(bool recalculateFromVerts) {
    vector<Vertex*>& selected = getSelected();

    if (selected.size() < 2) {
        resetFinalSelection();
        return;
    }

    if (recalculateFromVerts) {
        int left = display->getWidth(), right = 0, top = 0, bottom = display->getHeight();
        float unitX, unitY;
        int ix, iy;

        bool wrapsPhase = getRasterizer()->wrapsVertices();

        for(auto vertex : selected) {
            unitX = vertex->values[dims.x];
            unitY = vertex->values[dims.y];

            if (dims.y == Vertex::Phase || dims.x == Vertex::Phase && wrapsPhase) {
                float& phase = dims.x == Vertex::Phase ? unitX : unitY;

                if(phase > 1) {
                    phase -= 1;
                }
            }

            ix = roundToInt(panel->sx(unitX));
            iy = roundToInt(panel->sy(unitY));

            if (ix < left) {
                left = ix;
            }

            if (ix > right) {
                right = ix;
            }

            if (iy < bottom) {
                bottom = iy;
            }

            if (iy > top) {
                top = iy;
            }
        }

        finalSelection.setBounds(left, bottom, right - left, top - bottom);
        finalSelection.expand(10, 10);

        if (finalSelection.getWidth() < 30) {
            finalSelection.expand(5, 0);
        }

        if(finalSelection.getHeight() < 30) {
            finalSelection.expand(0, 5);
        }

        if(finalSelection.getX() < 0) {
            finalSelection.translate(-finalSelection.getX() + 5, 0);
        }

        if(finalSelection.getRight() > panel->sxnz(1)) {
            finalSelection.translate(display->getWidth() - finalSelection.getRight() - 5, 0);
        }

        if(finalSelection.getY() < 0) {
            finalSelection.translate(0, -finalSelection.getY() + 5);
        }

        if(finalSelection.getBottom() > display->getHeight()) {
            finalSelection.translate(0, display->getHeight() - finalSelection.getBottom() - 5);
        }

        Rectangle<int>& r = finalSelection;

        selectionCorners.clear();
        selectionCorners.emplace_back(r.getX(), r.getY());
        selectionCorners.emplace_back(r.getX(), r.getY() + r.getHeight());
        selectionCorners.emplace_back(r.getX() + r.getWidth(), r.getY() + r.getHeight());
        selectionCorners.emplace_back(r.getX() + r.getWidth(), r.getY());
    }

    for (int i = 0; i < 4; ++i) {
        NumberUtils::constrain<float>(selectionCorners[i].x, 0.f, float(display->getWidth()));
        NumberUtils::constrain<float>(selectionCorners[i].y, 0.f, float(display->getHeight()));
    }

    Vertex2& a = selectionCorners[0];
    Vertex2& b = selectionCorners[1];
    Vertex2& c = selectionCorners[2];
    Vertex2& d = selectionCorners[3];

   #if USE_CORNERS
    int cornerSize = 10;
    roundedCorners.clear();
    roundedCorners.push_back(getSpacedVertex(cornerSize, a, b));
    roundedCorners.push_back(getSpacedVertex(cornerSize, b, a));
    roundedCorners.push_back(getSpacedVertex(cornerSize, b, c));
    roundedCorners.push_back(getSpacedVertex(cornerSize, c, b));

    roundedCorners.push_back(getSpacedVertex(cornerSize, c, d));
    roundedCorners.push_back(getSpacedVertex(cornerSize, d, c));
    roundedCorners.push_back(getSpacedVertex(cornerSize, d, a));
    roundedCorners.push_back(getSpacedVertex(cornerSize, a, d));
   #endif

    Vertex2 selectCentre = (a + b + c + d) * 0.25f;

    panel->grabTex->rect.setPosition(selectCentre.x - 12, selectCentre.y - 12);
}

void Interactor::resetFinalSelection() {
    finalSelection.setBounds(-10, -10, 0, 0);
    selectionCorners.clear();
}

void Interactor::doDragMiddleZoom(const MouseEvent& e) {
    flag(SimpleRepaint) = true;

    float ratio = 1 - 0.1f * (state.currentMouse.y - state.lastMouse.y);
    ZoomRect& rect = panel->zoomPanel->rect;
    rect.w *= ratio;

    NumberUtils::constrain(rect.w, 0.001f, rect.xMaximum);
    rect.x = panel->invertScaleX(e.x) * (rect.xMaximum - rect.w);

    NumberUtils::constrain(rect.x, 0.f, rect.xMaximum - rect.w);
}

void Interactor::doBoxSelect(const MouseEvent& e) {
    int x  = e.getMouseDownX();
    int y  = e.getMouseDownY();
    int dx = e.getDistanceFromDragStartX();
    int dy = e.getDistanceFromDragStartY();

    vector<Vertex*>& selected = getSelected();

    bool removing = e.mods.isCommandDown();

    if (dx < 0) {
        x += dx;
        dx = -dx;
    }

    if (dy < 0) {
        y += dy;
        dy = -dy;
    }

    selection.setBounds(x, y, dx, dy);

    flag(SimpleRepaint) = true;

    vector<Vertex*> toErase, affectedVerts;

    bool doDimensionCheck = shouldDoDimensionCheck();
    MorphPosition morphPos = getModPosition();

    if (doDimensionCheck) {
        if (auto* itr3D = dynamic_cast<Interactor3D*>(this)) {
            const vector<SimpleIcpt>& interceptPairs = itr3D->getInterceptPairs();

            for (const auto& icpt : interceptPairs) {
                int xx = roundToInt(panel->sx(icpt.x));
                int yy = roundToInt(panel->sy(icpt.y));

                if (selection.contains(xx, yy) && icpt.parent != nullptr) {
                    Vertex2 adjustedXY(icpt.x, icpt.y + (icpt.isWrapped ? 1.f : 0.f));

                    Vertex* vert = findLinesClosestVertex(icpt.parent, adjustedXY, morphPos);
                    affectedVerts.push_back(vert);
                }
            }
        } else {
            const vector<Intercept>& controlPoints = getRasterizer()->getRastData().intercepts;

            for (const auto& icpt : controlPoints) {
                int xx = roundToInt(panel->sx(icpt.x));
                int yy = roundToInt(panel->sy(icpt.y));

                if (selection.contains(xx, yy) && icpt.cube != nullptr) {
                    Vertex* vert = icpt.cube->findClosestVertex(morphPos);
                    affectedVerts.push_back(vert);
                }
            }
        }
    } else {
        if (Mesh* mesh = getMesh()) {
            for(auto vert : mesh->getVerts()) {
                int xx = roundToInt(panel->sx(vert->values[dims.x]));
                int yy = roundToInt(panel->sy(vert->values[dims.y]));

                if (selection.contains(xx, yy)) {
                    affectedVerts.push_back(vert);
                }
            }
        }
    }

    for (auto vert : affectedVerts) {
        if (vert == nullptr) {
            continue;
        }

        if (isDuplicateVertex(vert)) {
            if (removing) {
                auto position = std::find(selected.begin(), selected.end(), vert);

                if (position != selected.end())
                    selected.erase(position);
            }
        } else if (! removing) {
            selected.push_back(vert);
        }
    }

    resizeFinalBoxSelection();
}

void Interactor::doDragCorner(const MouseEvent& e) {
    if (mouseFlag(FirstMove)) {
        vertexTransformUndoer.start();
    }

    int corner             = getStateValue(HighlitCorner);
    bool pivotAtCentre     = e.mods.isCommandDown();
    float boxUnitW         = panel->invertScaleXNoZoom(finalSelection.getWidth());
    float boxUnitH         = panel->invertScaleYNoZoom(finalSelection.getHeight()) - 1.f;
    Vertex2 disp        = state.currentMouse - state.start;
    ZoomRect& rect         = panel->zoomPanel->rect;

    float squashFactor = 1, unitOrigin = 0, origin = 0;

    if (corner != PanelState::MoveHandle) {
        disp.x /= rect.w;
        disp.y /= rect.h;
    }

    if (corner == PanelState::MoveHandle) {
        for (auto& frame : state.selectedFrame) {
            Vertex* vert = frame.vert;

            vert->values[dims.x] = frame.origin.x + disp.x;
            vert->values[dims.y] = frame.origin.y + disp.y;

            Range<float> xLimits = getVertexPhaseLimits(vert);

            NumberUtils::constrain<float>(vert->values[dims.x], xLimits);
            NumberUtils::constrain<float>(vert->values[dims.y], vertexLimits[dims.y]);
        }
    } else if (corner == PanelState::LeftPivot || corner == PanelState::RightPivot) {
        if (corner == PanelState::LeftPivot) {
            squashFactor = 1 - disp.x / boxUnitW;
            unitOrigin      = state.pivots[PanelState::RightPivot].x;
        }

        if (corner == PanelState::RightPivot) {
            squashFactor = 1 + disp.x / boxUnitW;
            unitOrigin   = state.pivots[PanelState::LeftPivot].x;
        }

        if (pivotAtCentre)
            unitOrigin = state.pivots[PanelState::CentrePivot].x;

        for (auto& it : state.selectedFrame) {
            Vertex* vert     = it.vert;
            float& val         = vert->values[dims.x];
            val             = (it.origin.x - unitOrigin) * squashFactor + unitOrigin;

            Range<float> xLimits = getVertexPhaseLimits(vert);
            NumberUtils::constrain<float>(val, xLimits);
        }

        origin = panel->sx(unitOrigin);

        for (int i = 0; i < (int) selectionCorners.size(); ++i)
            selectionCorners[i].x = (state.cornersStart[i].x - origin) * squashFactor + origin;
    } else if (corner == PanelState::TopPivot || corner == PanelState::BottomPivot) {
        if (corner == PanelState::TopPivot) {
            squashFactor = 1 + disp.y / boxUnitH;
            unitOrigin = state.pivots[PanelState::TopPivot].y;
        }

        if (corner == PanelState::Bot) {
            squashFactor = 1 - disp.y / boxUnitH;
            unitOrigin = state.pivots[PanelState::BottomPivot].y;
        }

        if(pivotAtCentre)
            unitOrigin = state.pivots[PanelState::CentrePivot].y;

        if(fabsf(squashFactor) < 0.001f)
            squashFactor = squashFactor < 0 ? -0.001f : 0.001f;

        for (auto& frame : state.selectedFrame) {
            float& val = frame.vert->values[dims.y];

            val = (frame.origin.y - unitOrigin) * squashFactor + unitOrigin;
            NumberUtils::constrain<float>(val, vertexLimits[dims.y]);
        }

        origin = panel->sy(unitOrigin);

        for (int i = 0; i < (int) selectionCorners.size(); ++i) {
            selectionCorners[i].y = (state.cornersStart[i].y - origin) * squashFactor + origin;
        }
    }

    if (!collisionDetector.validate()) {
        for (auto &frame: state.selectedFrame) {
            frame.vert->values[dims.x] = frame.lastValid.x;
            frame.vert->values[dims.y] = frame.lastValid.y;
        }
    } else {
        for (auto &frame: state.selectedFrame) {
            frame.lastValid = Vertex2(frame.vert->values[dims.x],
                                      frame.vert->values[dims.y]);

            flag(DidMeshChange) = true;
        }
    }

    flag(LoweredRes) = !state.selectedFrame.empty();
    resizeFinalBoxSelection(corner == PanelState::MoveHandle);
}

void Interactor::doDragPaintTool(const MouseEvent& e) {
    ScopedLock sl(vertexLock);

    if (pencilPath.size() < 2048) {
        if (pencilPath.empty()) {
            pencilPath.push_back(state.lastMouse);
            pencilPath.push_back(state.currentMouse);
        } else {
            Vertex2& current = pencilPath.back();
            Vertex2 next = current + (state.currentMouse - current) * realValue(PencilSmooth);

            pencilPath.push_back(next);
        }

        flag(SimpleRepaint) = true;
    }
}


void Interactor::setMouseDownStateSelectorTool(const MouseEvent& e) {
    PanelState::ActionState& action = state.actionState;

    bool selectWithRight = getSetting(SelectWithRight) == 1;

    if (e.mods.isLeftButtonDown()) {
        if (e.mods.isShiftDown() && selectWithRight) {
            action = PanelState::BoxSelecting;
        } else if (getStateValue(HighlitCorner) >= 0) {
            action = PanelState::DraggingCorner;
        } else if (e.mods.isCommandDown() || mouseFlag(WithinReshapeThresh)) {
            action = PanelState::ReshapingCurve;
        } else {
            jassert(! finalSelection.contains(e.getPosition()));

            //left click + not over playback line or zoombars
            action = selectWithRight ? PanelState::CreatingVertex : PanelState::ClickSelecting;
        }
    } else if (e.mods.isRightButtonDown()) {
        mouseFlag(WithinReshapeThresh) = false;

        if (e.mods.isShiftDown() && !selectWithRight) {
            action = PanelState::BoxSelecting;
        } else if (finalSelection.contains(e.getPosition()) && !e.mods.isShiftDown()) {
            getSelected().clear();
            state.selectedFrame.clear();

            resetFinalSelection();

            flag(SimpleRepaint) = true;
            getStateValue(HighlitCorner) = -1;
            action = PanelState::Idle;
        } else if (e.mods.isCommandDown()) {
            action = PanelState::SelectingConnected;
        } else {
            action = selectWithRight ? PanelState::ClickSelecting : PanelState::CreatingVertex;
        }
    } else if (e.mods.isMiddleButtonDown()) {
        action = PanelState::MiddleZooming;
    }
}


void Interactor::doDragVertex(const MouseEvent& e) {
    if (mouseFlag(FirstMove))
        vertexTransformUndoer.start();

    Vertex2 diff = state.currentMouse - state.lastMouse;

    if(e.mods.isCommandDown())
        diff.x = 0;
    else if(e.mods.isShiftDown())
        diff.y = 0;

    if(diff.x != 0 || diff.y != 0)
        flag(LoweredRes) = ! getSelected().empty();

    moveSelectedVerts(diff);

    listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
}

void Interactor::doReshapeCurve(const MouseEvent& e) {
}

void Interactor::setHighlitCorner(const MouseEvent& e, bool& wroteMessage) {
    getStateValue(HighlitCorner) = -1;

    for (int i = 0; i < (int) selectionCorners.size(); ++i) {
        Vertex2& v1 = selectionCorners[i];
        Vertex2& v2 = selectionCorners[(i + 1) % selectionCorners.size()];

        CurveLine line(v1, v2);
        float dist = line.distanceToPoint(Vertex2(e.x, e.y));

        if (NumberUtils::within(dist, 0.f, 8.f)) {
            getStateValue(HighlitCorner) = i;
            flag(SimpleRepaint) = true;

            break;
        }
    }

    if (getStateValue(HighlitCorner) < 0) {
        if (finalSelection.contains(e.getPosition())) {
            getStateValue(HighlitCorner) = PanelState::MoveHandle;

            repo->getConsole().setMouseUsage(true, false, false, true);
            repo->getConsole().setKeys("a");
            showCritical("Dismiss with right click (without shift)");
            wroteMessage = true;
        }
    }
}

void Interactor::doMouseUpPencil(const MouseEvent& e) {
    ScopedLock sl(vertexLock);

    pencilPath.push_back(state.currentMouse);
    commitPath(e);
    pencilPath.clear();

    flag(SimpleRepaint) = true;
}

void Interactor::loopBacktrack(
        vector<Vertex*>& loop,
        set<Vertex*>& alreadySeen,
        Vertex* current,
        int level) {
    if (exitBacktrackEarly || (current == loop[0] && level > 2)) {
        return;
    }

    for (int s = 0; s < current->owners.size(); ++s) {
        Vertex* v = current->owners[s]->getOtherVertexAlong(dims.x, current);

        if (v == loop[0] && level > 2) {
            exitBacktrackEarly = true;
            return;
        }

        if (alreadySeen.find(v) == alreadySeen.end()) {
            alreadySeen.insert(v);
            loop.push_back(v);
            loopBacktrack(loop, alreadySeen, v, level + 1);

            if(! exitBacktrackEarly) {
                loop.pop_back();
            }
        }
    }
}

void Interactor::validateMesh() {
    Mesh* mesh = getMesh();
    mesh->validate();

    locateClosestElement();
}

void Interactor::validateLinePhases() {
    Mesh* mesh = getMesh();

    if (getRasterizer()->wrapsVertices()) {
        for (auto line : mesh->getCubes()) {
            bool allPhaseAboveOne = true;
            bool anyPhaseNegative = false;

            for(int j = 0; j < TrilinearCube::numVerts; ++j) {
                Vertex* vert = line->getVertex(j);
                float phase = vert->values[Vertex::Phase];

                allPhaseAboveOne &= phase >= 1;
                anyPhaseNegative |= phase < 0;

                // we got big problems if this happens
                jassert(phase < 2);
            }

            if (allPhaseAboveOne || anyPhaseNegative) {
                for (int j = 0; j < TrilinearCube::numVerts; ++j) {
                    Vertex* vert = line->getVertex(j);

                    vert->values[Vertex::Phase] += (allPhaseAboveOne ? -1 : 1);
                }
            }
        }
    }
}

void Interactor::transferLineProperties(TrilinearCube const* from, TrilinearCube* to1, TrilinearCube* to2) {
    to1->setPropertiesFrom(from);
    to2->setPropertiesFrom(from);
}

void Interactor::selectConnectedVerts(set<Vertex*>& alreadySeen, Vertex* current) {
    alreadySeen.insert(current);
    vector<Vertex*>& selected = getSelected();

    for (auto cube : current->owners) {
        if (cube == nullptr)
            continue;

        Vertex* v = cube->getOtherVertexAlong(dims.x, current);

        if (alreadySeen.find(v) == alreadySeen.end()) {
            alreadySeen.insert(v);
            selected.push_back(v);
            selectConnectedVerts(alreadySeen, v);
        }
    }
}

void Interactor::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        updateDepthVerts();
        display->repaint();
    }

    if (updateType == Repaint) {
        display->repaint();
    }
}

void Interactor::reduceDetail() {
    getObj(Updater).update(getUpdateSource(), ReduceDetail);
}

void Interactor::restoreDetail() {
    getObj(Updater).update(getUpdateSource(), RestoreDetail);
}

void Interactor::doGlobalUIUpdate(bool force) {
    getObj(Updater).update(getUpdateSource(), Update);
}

void Interactor::updateDspSync() {
    rasterizer->performUpdate(Update);
}

void Interactor::postUpdateMessage() {
    updateDspSync();
    performUpdate(Update);

    if (isCurrentMeshActive() && getSetting(UpdateGfxRealtime) && extraUpdateCondition()) {
        triggerRefreshUpdate();
    }
}

void Interactor::snapToGrid(Vertex2& toSnap) {
    int sizeX = NumberUtils::nextPower2(roundToInt(0.7 * panel->vertMinorLines.size()));
    int sizeY = NumberUtils::nextPower2(roundToInt(0.7 * panel->horzMinorLines.size()));

    toSnap.x *= sizeX;
    toSnap.x  = (float)(int)(toSnap.x + 0.5f);
    toSnap.x /= sizeX;

    toSnap.y *= sizeY;
    toSnap.y  = (float)(int)(toSnap.y + 0.5f);
    toSnap.y /= sizeY;
}

void Interactor::setRasterizer(OldMeshRasterizer* rasterizer) {
    this->rasterizer = rasterizer;
    this->rasterizer->setDims(dims);
}

float Interactor::getDragMovementScale(TrilinearCube* cube) {
    float scale = 1.f;

    if (cube) {
        Vertex* vNear = findLinesClosestVertex(cube, state.currentMouse);

        for (int i = 0; i < dims.numHidden(); ++i) {
            Vertex* vFar    = cube->getOtherVertexAlong(dims.hidden[i], vNear);
            float dimValue  = positioner->getValue(dims.hidden[i]);
            float distA     = fabsf(dimValue - vNear->values[dims.hidden[i]]);
            float distB     = fabsf(dimValue - vFar->values[dims.hidden[i]]);
            float maxDist   = jmax(distA, distB);
            float distNearFar = fabsf(vNear->values[dims.hidden[i]] - vFar->values[dims.hidden[i]]);

            scale *= (maxDist < 0.0001f) ? 1.f : distNearFar / maxDist;
        }
    }

    return scale;
}

void Interactor::updateDepthVerts() {
    Mesh* mesh = getMesh();

    if (mesh == nullptr) {
        return;
    }

    vector<float> hiddenDepths    (dims.numHidden());
    vector<float> hiddenBaseValues(dims.numHidden());

    for (int i = 0; i < dims.numHidden(); ++i) {
        hiddenDepths[i]     = positioner->getDepth(dims.hidden[i]);
        hiddenBaseValues[i] = positioner->getValue(dims.hidden[i]);
    }

    if(dims.numHidden() == 0 || mesh->getNumVerts() == 0) {
        return;
    }

    depthVerts.clear();

    for (auto vert : mesh->getVerts()) {
        bool contained = true;

        for (int i = 0; i < dims.numHidden(); ++i) {
            contained &= NumberUtils::within(vert->values[dims.hidden[i]], hiddenBaseValues[i],
                                             hiddenBaseValues[i] + hiddenDepths[i]);
        }

        if (contained && vert->getNumOwners() == 0) {
            float alpha = 1.f;

            for (int i = 0; i < dims.numHidden(); ++i) {
                // diminish alpha of more distant verts
                if (hiddenDepths[i] != 0) {
                    alpha *= (vert->values[dims.hidden[i]] - hiddenBaseValues[i]) / hiddenDepths[i];
                }

                alpha *= alpha;
                alpha = jmax(0.f, 1.f - jmin(1.f, alpha));
            }

            depthVerts.emplace_back(vert->values[dims.x], vert->values[dims.y], vert, alpha);
        }
    }
}

void Interactor::moveSelectedVerts(const Vertex2& diff) {
    vector<Vertex*>& selected = getSelected();

    if(selected.empty()) {
        return;
    }

    if(state.currentVertex == nullptr) {
        return;
    }

    vector<VertexFrame>& movingX     = state.singleHorz;
    vector<VertexFrame>& movingXY     = state.singleXY;
    vector<VertexFrame>& movingAll    = state.singleAll;

    Vertex2 candidate(state.currentVertex->values[dims.x], state.currentVertex->values[dims.y]);
    Vertex2 old(candidate);
    candidate += diff;

    Vertex2 newXY(candidate);
    bool snap = getSetting(SnapMode) && selected.size() == 1;

    if (snap) {
        Vertex2 mouseDelta(state.currentMouse - state.start);
        if (ModifierKeys::getCurrentModifiers().isCommandDown()) {
            mouseDelta.x = 0;
        } else if (ModifierKeys::getCurrentModifiers().isShiftDown()) {
            mouseDelta.y = 0;
        }

        for (auto & it : movingXY) {
            Vertex* vert = it.vert;

            if (vert == state.currentVertex) {
                Vertex2 newPos = it.origin + mouseDelta;
                snapToGrid(newPos);

                newXY     = newPos;
                candidate = newPos;

                break;
            }
        }
    }

    Range<float> phaseLimits = getVertexPhaseLimits(state.currentVertex);

    NumberUtils::constrain(newXY.x, phaseLimits);
    NumberUtils::constrain(newXY.y, vertexLimits[dims.y]);

    Vertex2 limitedDiff = newXY - old;
    Vertex2 bestNew(newXY);
    Vertex2 bestOld(old);
    Vertex2 last;

    Mesh* mesh     = getMesh();
    bool testY     = is3DInteractor();
    bool passed = true;

    translateVerts(movingXY, limitedDiff);
    translateVerts(movingX, Vertex2(limitedDiff.x, 0));

    if (!shouldDoDimensionCheck()) {
        flag(DidMeshChange) |= !(candidate == old);
        return;
    }

    collisionDetector.setCurrentSelection(mesh, movingAll);
    passed = collisionDetector.validate();
    Range<float> yLimits = vertexLimits[dims.y];

    if (!passed) {
        Vertex2 currentDiff;
        const int refiningIterations = 8;
        const float diffThres = 0.002;

        // reset before refining
        bestNew = newXY;
        bestOld = old;
        candidate = old;

        passed = true;

        for (int i = 0; i < refiningIterations; ++i) {
            Vertex2 targetValue     = passed ? bestNew : bestOld;
            Vertex2& valueToUpdate     = passed ? bestOld : bestNew;

            valueToUpdate     = candidate;
            candidate         = (candidate + targetValue) * 0.5f;
            currentDiff     = candidate - old;

            // move vertices
            for (auto& allIter : movingAll) {
                float newPos = allIter.lastValid.x + currentDiff.x;
                NumberUtils::constrain(newPos, phaseLimits);
                allIter.vert->values[dims.x] = newPos;
            }

            if (testY) {
                // note that the first movingXY.size() of startpositions array corresponds to the same vertices
                for (auto& xyIter : movingXY) {
                    float newPos = xyIter.lastValid.y + currentDiff.y;
                    NumberUtils::constrain(newPos, yLimits);
                    xyIter.vert->values[dims.y] = newPos;
                }
            }

            // see if there are still no collisions
            passed = collisionDetector.validate();

            Vertex2 diffToLast = last - candidate;

            if (passed && diffToLast.isSmallerThan(diffThres)) {
                break;
            }

            last = candidate;
        }

        if (!passed) {
            candidate = old;

            // reset positions to start
            for (auto& frame : movingAll) {
                frame.vert->values[dims.x] = frame.lastValid.x;
            }

            if (testY) {
                for (auto& frame : movingXY) {
                    frame.vert->values[dims.y] = frame.lastValid.y;
                }
            }

            jassert(collisionDetector.validate());
        }

        // if y movements are free from possibility of collision
        if (!testY) {
            for (auto& frame : movingXY) {
                float& val = frame.vert->values[dims.y];

                val += limitedDiff.y;
                NumberUtils::constrain(val, yLimits);
            }
        }
    }

    if (passed) {
        updateLastValid(movingX);
        updateLastValid(movingXY);
        updateLastValid(movingAll);
    }

    validateLinePhases();

    // only set flag if we've actually moved the damned verts after all that
    flag(DidMeshChange) |= ! (candidate == old);
}

void Interactor::updateLastValid(vector<VertexFrame>& verts) const {
    for(auto& frame : verts) {
        frame.lastValid = Vertex2(frame.vert->values[dims.x], frame.vert->values[dims.y]);
    }
}

void Interactor::translateVerts(vector<VertexFrame>& verts, const Vertex2& diff) {
    float proximityThresh = 0.001f;
    bool is3D = is3DInteractor();

    for (auto& it : verts) {
        Vertex* vert = it.vert;
        TrilinearCube* line     = getClosestLine(vert);

        Range<float> xLimits = getVertexPhaseLimits(vert);

        float& x     = vert->values[dims.x];
        float oldX     = x;
        float newX     = x + diff.x;

        NumberUtils::constrain(newX, xLimits);

        // to prevent divisions by 0
        if (is3D && line != nullptr) {
            Vertex* oppVert = line->getOtherVertexAlong(dims.x, vert);

            if(oppVert != nullptr) {
                float oppX = oppVert->values[dims.x];

                if(fabsf(oppX - newX) < proximityThresh) {
                    newX = (oldX > oppX) ? oppX + proximityThresh : oppX - proximityThresh;
                }
            }
        }

        x = newX;

        vert->values[dims.y] += diff.y;
        NumberUtils::constrain(vert->values[dims.y], vertexLimits[dims.y]);
    }
}

Mesh* Interactor::getMesh() {
    return getObj(MeshLibrary).getCurrentMesh(layerType);
}

void Interactor::doClickSelect(const MouseEvent& e) {
    Vertex* v = state.currentVertex;

    if(v == nullptr) {
        v = findClosestVertex();
    }

    if(v == nullptr) {
        return;
    }

    bool wrapsPhase = getRasterizer()->wrapsVertices();

    float x      = v->values[dims.x];
    float y      = v->values[dims.y];
    int dimArr[] = { dims.x, dims.y };
    float vals[] = { x, y };

    if (wrapsPhase) {
        for (int i = 0; i < numElementsInArray(dimArr); ++i) {
            if (dimArr[i] == Vertex::Phase && vals[i] > 1)
                vals[i] -= 1;
        }
    }

    if (e.mods.isShiftDown()) {
        addSelectedVertexOrToggle(v);
    } else {
        vector<Vertex*>& selected = getSelected();

        if (!isDuplicateVertex(state.currentVertex)) {
            selected.clear();
            selected.push_back(v);
        }
    }

    updateSelectionFrames();

    if(Interactor* opposite = getOppositeInteractor()) {
        opposite->performUpdate(Update);
    }
}

bool Interactor::doCreateVertex() {
    auto* vertex = new Vertex();

    vertex->values[Vertex::Phase] = 0.5f;
    vertex->values[Vertex::Amp] = 0.5f;

    for(int i = 0; i < dims.numHidden(); ++i)
        vertex->values[dims.hidden[i]] = positioner->getValue(dims.hidden[i]);

    vertex->values[dims.x] = state.currentMouse.x;
    vertex->values[dims.y] = state.currentMouse.y;

    Mesh* mesh = getMesh();

    if(mesh == nullptr) {
        return false;
    }

    vector<Vertex*> before = mesh->getVerts();

    {
        ScopedLock sl(vertexLock);

        state.currentVertex = vertex;
        mesh->addVertex(vertex);
        vector<Vertex*> after = mesh->getVerts();

        auto* addVertexAction = new UpdateVertexVectorAction(this, &mesh->getVerts(), before, after, false);
        getObj(EditWatcher).addAction(addVertexAction);

        vector<Vertex*>& selected = getSelected();

        selected.clear();
        selected.push_back(state.currentVertex);

        resetFinalSelection();
        updateSelectionFrames();
    }

    // if we've got more dimensions, this isn't creating a line, so won't require a rerasterize
    if (dims.numHidden() > 0) {
        flag(SimpleRepaint) = true;
        performUpdate(Update);

        if(Interactor* opposite = getOppositeInteractor()) {
            opposite->performUpdate(Update);
        }
    } else {
        // however if we do not, rasterization only needs these two axes (x and y)
        flag(DidMeshChange) = true;
    }

    return true;
}

MouseUsage Interactor::getMouseUsage() {
    return MouseUsage(true, true, true, true);
}

float Interactor::getVertexClickProximityThres() {
    return 0.06f * panel->zoomPanel->rect.w;
}

void Interactor::refresh() {
    if (flag(DidMeshChange)) {
        if (doesMeshChangeWarrantGlobalUpdate()) {
            postUpdateMessage();

            // loweredres being true implies we dragged, and if if we haven't,
            // we can clear the rasterize flag to prevent a redundant refresh on mouse up
            bool rasterizeOnMouseUp = flag(LoweredRes); // && getSetting(ReductionFactor) != 1;
            flag(DidMeshChange) = rasterizeOnMouseUp;
        } else {
            updateDspSync();
            performUpdate(Update);

            if (Interactor * opposite = getOppositeInteractor())
                opposite->performUpdate(Update);
        }
    } else if (flag(SimpleRepaint)) {
        display->repaint();
        flag(SimpleRepaint) = false;
    }
}

bool Interactor::doesMeshChangeWarrantGlobalUpdate() {
    if (!isCurrentMeshActive()) {
        return false;
    }

    return true;
}

vector<Vertex*>& Interactor::getSelected() {
    return getObj(MeshLibrary).getSelectedByType(layerType);
}

bool Interactor::commitCubeAdditionIfValid(TrilinearCube*& addedCube,
                                           const vector<TrilinearCube*>& beforeCubes,
                                           const vector<Vertex*>& beforeVerts) {
    Mesh* mesh = getMesh();

    // use reference because these will be deep copied in the
    // UpdateVertexVectorAction ctor, so no need to deep copy them here
    vector<TrilinearCube*>& afterCubes = mesh->getCubes();
    vector<Vertex*>& afterVerts = mesh->getVerts();

    afterCubes.push_back(addedCube);

    collisionDetector.setCurrentSelection(mesh, addedCube);
    bool passed = collisionDetector.validate();

    if (passed) {
        bool haveCubes = dims.numHidden() > 0;
        const bool doUpdateWithVertexAction = beforeCubes.size() == afterCubes.size() || ! haveCubes;

        getObj(EditWatcher).addAction(new UpdateVertexVectorAction(this, &mesh->getVerts(), beforeVerts,
                                                                      afterVerts, doUpdateWithVertexAction));

        if(haveCubes) {
            getObj(EditWatcher).addAction(new UpdateCubeVectorAction(this, &mesh->getCubes(), beforeCubes, afterCubes), false);
        }

        // undoable action triggers the update
        flag(DidMeshChange) = false;

        ScopedLock sl(vertexLock);

        state.currentVertex = findLinesClosestVertex(addedCube, state.currentMouse);
        state.currentCube = addedCube;

        return true;
    }

    afterCubes = beforeCubes;
    afterVerts = beforeVerts;

    ScopedLock sl(vertexLock);
    addedCube->orphanVerts();

    delete addedCube;

    state.currentVertex = nullptr;
    state.currentCube = nullptr;

    showImportant("Could not add line due to collision");

    return false;
}

vector<TrilinearCube*> Interactor::getLinesToSlideOnSingleSelect() {
    return {};
}

Array<Vertex*> Interactor::getVerticesToMove(TrilinearCube* cube, Vertex* startVertex) {
    Array<Vertex*> movingVerts;

    if(startVertex != nullptr) {
        if (cube != nullptr && dims.numHidden() > 0) {
            bool linkYllw  = getSetting(LinkYellow) == 1;
            bool linkRed   = getSetting(LinkRed)    == 1;
            bool linkBlue  = getSetting(LinkBlue)   == 1;

            int numLinks   = int(linkYllw) + int(linkRed) + int(linkBlue);

            if(numLinks == 0) {
                movingVerts.add(startVertex);
            } else if(numLinks == 1) {
                int dim = (linkYllw ? Vertex::Time : linkRed ? Vertex::Red : Vertex::Blue);

                movingVerts.add(startVertex);
                movingVerts.add(cube->getOtherVertexAlong(dim, startVertex));
            } else if (numLinks == 2) {
                int dim = (linkYllw && linkRed) ? Vertex::Blue : (linkYllw && linkBlue) ? Vertex::Red
                                                                                        : Vertex::Time;
                TrilinearCube::Face face = cube->getFace(dim, startVertex);
                movingVerts = face.toArray();
            } else if (numLinks == 3) {
                movingVerts = cube->toArray();
            }
        } else {
            movingVerts.add(startVertex);
        }
    }

    return movingVerts;
}

void Interactor::setMovingVertsFromSelected() {
    state.selectedFrame.clear();
    vector<Vertex*>& selected = getSelected();

    for (auto& it : selected) {
        addToArray(getVerticesToMove(getClosestLine(it), it), state.selectedFrame);
    }
}

MorphPosition Interactor::getModPosition(bool adjust) {
    MorphPosition m = positioner->getMorphPosition();

    if (adjust) {
        if (ignoresTime)
            m.time = 0;

        if (scratchesTime) {
            if (positioner->getPrimaryDimension() == Vertex::Time) {
                int chan = panel->getLayerScratchChannel();

                if (chan != CommonEnums::Null)
                    m.time = positioner->getDistortedTime(chan);
            }
        }
    }

    return m;
}

Range<float> Interactor::getVertexPhaseLimits(Vertex* vert) {
    return vertexLimits[dims.x];
}

bool Interactor::addNewCube(float startTime, float phase, float amp, float curveShape) {
    return addNewCube(startTime, phase, amp, curveShape, getCube());
}

bool Interactor::addNewCube(float startTime, float phase, float amp, float curveShape, const MorphPosition& box) {
    ScopedLock sl(vertexLock);

    vector<Intercept> icpts3D;
    bool is3D = false;

    if (is3DInteractor()) {
        if (Interactor * itr2D = getOppositeInteractor()) {
            icpts3D = itr2D->getRasterizer()->getRastData().intercepts;
            is3D = true;
        }
    }

    if(! positioner->usesLineDepthFor(dims.x)) {
        startTime = 0;
    }

    Mesh* mesh = getMesh();

    if(mesh == nullptr) {
        return false;
    }

    vector<Vertex*> beforeVerts;
    vector<TrilinearCube*> beforeCubes;

    if(! suspendUndo) {
        mesh->copyElements(beforeVerts, beforeCubes);
    }

    auto* addedLine = new TrilinearCube(mesh);

    const vector<Intercept>& controlPoints = is3D ? icpts3D : rasterizer->getRastData().intercepts;

    if (controlPoints.empty()) {
        initVertsWithDepthDims(addedLine, dims.hidden, phase, amp, box);
    } else if (controlPoints.size() == 1) {
        addNewCubeForOneIntercept(addedLine, startTime, phase, amp, box);
    } else {
        addNewCubeForMultipleIntercepts(addedLine, startTime, phase, amp);
    }

    // shorten line's span to fit current depths
    if (!controlPoints.empty()) {
        float times[] = {box.time, box.timeEnd()};
        float reds[] = {box.red, box.redEnd()};
        float blues[] = {box.blue, box.blueEnd()};

        int vertexIdx = 0;

        for (float time : times) {
            for (float red : reds) {
                for (float blue : blues) {
                    addedLine->getFinalIntercept(reduceData, MorphPosition(time, red, blue));

                    Vertex* vertex = addedLine->getVertex(vertexIdx);

                    vertex->values[Vertex::Time] = time;
                    vertex->values[Vertex::Red]  = red;
                    vertex->values[Vertex::Blue] = blue;

                    // voodoo warning: final is updated on getLineIntercept()
                    if(! is3D) {
                        vertex->values[dims.x] = reduceData.v.values[dims.x];
                    }

                    vertex->values[dims.y] = reduceData.v.values[dims.y];

                    ++vertexIdx;
                }
            }

            if(vertexIdx >= TrilinearCube::numVerts) {
                break;
            }
        }
    }

    for (int i = 0; i < TrilinearCube::numVerts; ++i) {
        addedLine->getVertex(i)->addOwner(addedLine);

        if (curveShape >= 0) {
            addedLine->getVertex(i)->values[Vertex::Curve] = curveShape;
        }
    }

    validateLinePhases();
    adjustAddedLine(addedLine);

    bool succeeded = true;
    if (suspendUndo) {
        mesh->addCube(addedLine);
    } else {
        succeeded = commitCubeAdditionIfValid(addedLine, beforeCubes, beforeVerts);
    }

    return succeeded;
}


void Interactor::addNewCubeForOneIntercept(
        TrilinearCube* addedLine,
        float startTime,
        float phase,
        float amp,
        const MorphPosition& box) {
    Mesh* mesh = getMesh();

    jassert(!(mesh->getNumCubes() > 0));

    TrilinearCube* meshLine = *mesh->getCubes().begin();

    // get difference from the line's interpolated mod position to our click point
    // used then to shift all the copied verts, from that line, by the difference
    // unless of course the existing line doesn't overlap our mod position, in which
    // case we create a normal depth cube configuration
    meshLine->getFinalIntercept(reduceData, getModPosition());

    if (reduceData.pointOverlaps) {
        float diffPhase = phase - reduceData.v.values[Vertex::Phase];

        for (int i = 0; i < TrilinearCube::numVerts; ++i) {
            Vertex* vert = addedLine->getVertex(i);
            *vert = *meshLine->getVertex(i);

            vert->values[Vertex::Phase] += diffPhase;
            vert->values[Vertex::Amp] = amp;
        }
    } else {
        // we're at a position where the existing line doesn't overlap with our mod position
        // so just create a vanilla line cube
        initVertsWithDepthDims(addedLine, dims.hidden, phase, amp, box);
    }
}

void Interactor::addNewCubeForMultipleIntercepts(
        TrilinearCube* addedCube,
        float startTime,
        float phase,
        float amp) {
    const vector<Intercept>& controlPoints = rasterizer->getRastData().intercepts;

    TrilinearCube* rightLine = nullptr;
    TrilinearCube* leftLine = nullptr;

    const Intercept* leftmostIcpt;
    const Intercept* rightmostIcpt;
    const Intercept* rightIcpt = nullptr;
    const Intercept* leftIcpt = nullptr;

    leftmostIcpt = &controlPoints.front();
    rightmostIcpt = &controlPoints.back();

    // x is not contains within range of intercepts
    if (controlPoints.size() > 1 && phase >= rightmostIcpt->x || phase <= leftmostIcpt->x) {
        leftLine     = leftmostIcpt->cube;
        rightLine    = rightmostIcpt->cube;
        leftIcpt     = leftmostIcpt;
        rightIcpt    = rightmostIcpt;
    } else {
        for (int i = 0; i < (int) controlPoints.size() - 1; ++i) {
            if (NumberUtils::within(phase, controlPoints[i].x, controlPoints[i + 1].x)) {
                leftIcpt  = &controlPoints[i];
                leftLine  = leftIcpt->cube;

                // cube can be null with envelope padding intercepts, so choose the one to the left of it
                int j = 1;
                while (leftLine == nullptr && i - j >= 0) {
                    leftIcpt = &controlPoints[i - j];
                    leftLine = leftIcpt->cube;
                }

                rightIcpt = &controlPoints[i + 1];
                rightLine = rightIcpt->cube;
            }
        }
    }

    jassert(leftLine && rightLine);

    if(leftLine == nullptr || rightLine == nullptr || leftIcpt == nullptr || rightIcpt == nullptr) {
        return;
    }

    // why can't I just use the EFFING intercept X value??
    float leftIcptPhs = leftIcpt->x;
    float rightIcptPhs = rightIcpt->x;

    bool wraps = rasterizer->wrapsVertices();

    if (!wraps && phase > rightIcptPhs || phase < leftIcptPhs) {
        TrilinearCube* toCopy = (phase > rightIcptPhs) ? rightLine : leftLine;
        float diffPhs      = phase - ((phase > rightIcptPhs) ? rightIcptPhs : leftIcptPhs);

        for (int i = 0; i < TrilinearCube::numVerts; ++i) {
            Vertex* vert = addedCube->getVertex(i);

            *vert = *toCopy->getVertex(i);
            vert->values[Vertex::Phase] += diffPhs;
        }
    } else {
        float leftWrapOffset       = 0;
        float rightWrapOffset      = 0;
        TrilinearCube* wrappedLeftLine  = leftLine;
        TrilinearCube* wrappedRightLine = rightLine;

        if (phase > rightIcptPhs) {
            leftWrapOffset = 1;
        } else if (phase < leftIcptPhs) {
            rightWrapOffset = -1;
        }

        float wrappedLeftIcptPhs  = leftIcptPhs + leftWrapOffset;
        float wrappedRightIcptPhs = rightIcptPhs + rightWrapOffset;

        float diffXFromLeft       = phase - wrappedLeftIcptPhs;
        float diffBetweenPoles    = wrappedLeftIcptPhs - wrappedRightIcptPhs;
        float portionOfLeft       = diffBetweenPoles == 0 ? 1 : 1 - fabsf(diffXFromLeft / diffBetweenPoles);

        for (int i = 0; i < TrilinearCube::numVerts; ++i) {
            Vertex* vert      = addedCube->getVertex(i);
            Vertex* rightVert = rightLine->getVertex(i);
            Vertex* leftVert  = leftLine->getVertex(i);

            *vert = *leftVert * portionOfLeft + *rightVert * (1 - portionOfLeft);

            // correct the phase values because they can wrap and mess up linear combination above
            vert->values[Vertex::Phase] =
                    (leftVert->values[Vertex::Phase] + leftWrapOffset) * portionOfLeft +
                    (rightVert->values[Vertex::Phase] + rightWrapOffset) * (1 - portionOfLeft);
        }
    }

    if(dims.y == Vertex::Amp) {
        for (int i = 0; i < TrilinearCube::numVerts; ++i) {
            Vertex* vert = addedCube->getVertex(i);

            vert->owners.clear(); // why?? xxx
            vert->values[Vertex::Amp] = amp;
        }
    }
}


void Interactor::initVertsWithDepthDims(
        TrilinearCube* cube,
        const vector<int>& depthDims,
        float phase,
        float amp,
        const MorphPosition& box) {
    cube->initVerts(box);

    for (int i = 0; i < TrilinearCube::numVerts; ++i) {
        cube->getVertex(i)->values[Vertex::Phase]     = phase;
        cube->getVertex(i)->values[Vertex::Amp]     = amp;
    }
}

MorphPosition Interactor::getCube() {
    MorphPosition pos = getOffsetPosition(true);

    const float minLength = getConstant(MinLineLength);

    pos.time.setValueDirect(jlimit(pos.time.getTargetValue(), 0.f, 1.f - minLength));
    pos.red.setValueDirect(jlimit(pos.red.getTargetValue(),   0.f, 1.f - minLength));
    pos.blue.setValueDirect(jlimit(pos.blue.getTargetValue(), 0.f, 1.f - minLength));
    NumberUtils::constrain(pos.timeDepth,   minLength, 1.f - pos.time);
    NumberUtils::constrain(pos.redDepth,    minLength, 1.f - pos.red);
    NumberUtils::constrain(pos.blueDepth,   minLength, 1.f - pos.blue);

    return pos;
}

Vertex* Interactor::findLinesClosestVertex(TrilinearCube* cube, const Vertex2& mouseXY, Vertex& pos) {
    float minDist = 100000;
    float dist;

    pos.values[dims.x] = mouseXY.x;

    // don't want to factor in amplitude position in 2D selection
    if(dims.y != Vertex::Amp)
        pos.values[dims.y] = mouseXY.y;

    Vertex* closest = 0;

    float invWidth     = 1 / float(display->getWidth());
    float invHeight = 1 / float(display->getHeight());
    float syPos     = panel->sy(pos[dims.y]);
    float sxPos     = panel->sx(pos[dims.x]);

    float diff;
    for (auto& lineVert : cube->lineVerts) {
        dist = 0;

        for (int i = 0; i < 5; ++i) {
            if(pos[i] < 0) {
                continue;
            }

            float val = lineVert->values[i];

            diff = pos[i] - val;

            if (i == dims.x) {
                diff = (sxPos - panel->sx(val)) * invWidth;
            } else if (i == dims.y) {
                diff = (syPos - panel->sy(val)) * invHeight;
            }

            dist += diff * diff;
        }

        if (dist < minDist) {
            minDist = dist;
            closest = lineVert;
        }
    }

    jassert(closest != nullptr);

    return closest;
}

Vertex* Interactor::findLinesClosestVertex(TrilinearCube* cube, const Vertex2& mouseXY) {
    return findLinesClosestVertex(cube, mouseXY, getModPosition());
}

Vertex* Interactor::findLinesClosestVertex(
        TrilinearCube* cube,
        const Vertex2& mouseXY,
        const MorphPosition& morph) {

    Vertex pos;
    pos[Vertex::Phase] = -1.f;
    pos[Vertex::Amp  ] = -1.f;
    pos[Vertex::Time ] = morph.time;
    pos[Vertex::Red  ] = morph.red;
    pos[Vertex::Blue ] = morph.blue;

    return findLinesClosestVertex(cube, mouseXY, pos);
}

void Interactor::clearSelectedAndCurrent() {
    getSelected().clear();
    state.selectedFrame.clear();
    state.singleAll.clear();
    state.singleHorz.clear();
    state.singleXY.clear();

    state.currentCube = nullptr;
    state.currentVertex = nullptr;

    listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
}

void Interactor::resetSelection() {
    getSelected().clear();
    state.selectedFrame.clear();
    state.singleHorz.clear();
    state.singleXY.clear();
    state.singleAll.clear();

    listeners.call(&InteractorListener::selectionChanged, getMesh(), state.selectedFrame);
}

void Interactor::setAxeSize(float size) {
    state.realValues[PanelState::PencilRadius] = size;
}

bool Interactor::shouldDoDimensionCheck() {
    return dims.numHidden() > 0;
}

TrilinearCube* Interactor::getClosestLine(Vertex* vert) {
    if (vert->getNumOwners() > 0) {
        MorphPosition pos = positioner->getMorphPosition();

        for (auto& owner : vert->owners) {
            owner->getFinalIntercept(reduceData, pos);

            if (reduceData.pointOverlaps) {
                return owner;
            }
        }

        float minPos = 10000;
        TrilinearCube* closest = nullptr;

        for (auto cube : vert->owners) {
            float dist = pos.distanceTo(cube->getCentroid(vertexProps.isEnvelope));
            if (dist < minPos) {
                closest = nullptr;
                minPos = dist;
            }
        }

        return closest;
    }

    return nullptr;
}
