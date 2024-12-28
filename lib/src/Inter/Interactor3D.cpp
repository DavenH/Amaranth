#include <algorithm>
#include <iterator>
#include "Interactor3D.h"
#include "../App/EditWatcher.h"
#include "../App/Settings.h"
#include "../Curve/EnvelopeMesh.h"
#include "../Curve/CollisionDetector.h"
#include "../Curve/SimpleIcpt.h"
#include "../Inter/UndoableMeshProcess.h"
#include "../Obj/Ref.h"
#include "../Thread/LockTracer.h"
#include "../UI/Panels/Panel.h"
#include "../UI/Panels/Panel3D.h"
#include "../Util/NumberUtils.h"
#include "../Util/CommonEnums.h"
#include "../Curve/SurfaceLine.h"
#include "../Curve/MeshRasterizer.h"
#include "../Design/Updating/Updater.h"
#include "../Definitions.h"

Interactor3D::Interactor3D(
        SingletonRepo* repo,
        const String& name,
        const Dimensions& d
    ) : Interactor(repo, name, d)
    , canDoMiddleScroll(false), SingletonAccessor(repo, name) {
}

bool Interactor3D::locateClosestElement() {
//  progressMark
    ScopedLock sl(vertexLock);

    float minDist = 1e7f;
    float dist = 0;

    int oldIcpt     = state.currentIcpt;
    int oldFreeVert = state.currentFreeVert;

    if (interceptPairs.empty() && depthVerts.empty()) {
        state.currentIcpt     = -1;
        state.currentFreeVert = -1;
        return false;
    }

    int closestDepthVertIdx = -1;
    int closestIcptIdx  = -1;
    float x             = state.currentMouse.x;
    float y             = state.currentMouse.y;
    bool depthIsClosest = true;

    float invWidth  = 1 / float(display->getWidth());
    float invHeight = 1 / float(display->getHeight());
    float sxPos     = panel->sx(x);
    float syPos     = panel->sy(y);

    int idx     = 0;
    for (auto& depthVert : depthVerts) {
        float diffX = (panel->sx(depthVert.x) - sxPos) * invWidth;
        float diffY = (panel->sy(depthVert.y) - syPos) * invHeight;

        dist        = diffX * diffX + diffY * diffY;

        if (dist < minDist) {
            minDist = dist;
            closestDepthVertIdx = idx;
        }
        ++idx;
    }

    idx = 0;
    for (auto & interceptPair : interceptPairs) {
        float diffX = (panel->sx(interceptPair.x) - sxPos) * invWidth;
        float diffY = (panel->sy(interceptPair.y) - syPos) * invHeight;
        dist        = diffX * diffX + diffY * diffY;

        if (dist < minDist) {
            depthIsClosest = false;
            minDist        = dist;
            closestIcptIdx = idx;
        }
        ++idx;
    }

    bool changed = false;

    if (depthIsClosest) {
        state.currentCube       = nullptr;
        state.currentVertex     = depthVerts[closestDepthVertIdx].vert;
        state.currentFreeVert   = closestDepthVertIdx;
        state.currentIcpt       = -1;

        changed = oldFreeVert != state.currentFreeVert;
    } else {
        SimpleIcpt& icpt = interceptPairs[closestIcptIdx];
        Vertex2 mousePos = state.currentMouse;

        if(icpt.isWrapped)
            mousePos.y += 1.f;

        state.currentCube       = icpt.parent;
        state.currentIcpt       = closestIcptIdx;
        state.currentFreeVert   = -1;
        state.currentVertex     = findLinesClosestVertex(state.currentCube, mousePos);

        changed = oldIcpt != state.currentIcpt;
    }

    flag(SimpleRepaint) |= changed;

    return changed;
}

void Interactor3D::doExtraMouseDown(const MouseEvent& e) {
    vector<Vertex*>& selected = getSelected();

    if(getSetting(Tool) == Tools::Axe)
        doAxe(e);

    else if (actionIs(SelectingConnected)) {
        ScopedLock sl(vertexLock);

        selected.clear();

        Vertex* v = state.currentVertex;

        if (v != nullptr) {
            set<Vertex*> seen;
            seen.insert(v);
            selected.push_back(v);
            exitBacktrackEarly = false;
            loopBacktrack(selected, seen, v, 0);

            if (selected.size() < 3) {
                selected.clear();
                selected.push_back(v);

                set<Vertex*> alreadySeen;
                selectConnectedVerts(alreadySeen, v);
            }
        }

        updateSelectionFrames();
    }
}


void Interactor3D::doDragPaintTool(const MouseEvent& e) {
    if (actionIs(PaintingCreate)) {
        /*
        float radiusSqr = pow(realValue(PencilSmooth), 2.f);
        float x, y, dist;

        MorphPosition pos = positioner->getOffsetPosition(true);

        float mx = state.currentMouse.x;
        float my = state.currentMouse.y;

        vector<Vertex*>& verts = getMesh()->verts;
        for(vector<Vertex*>::iterator it = verts.begin(); it != verts.end(); ++it)
        {
            if((dims.x == Vertex::Time &&
                    NumberUtils::within((**it)[Vertex::Red],  pos.red,  pos.redEnd()) &&
                    NumberUtils::within((**it)[Vertex::Blue], pos.blue, pos.blueEnd())) ||
                (dims.x == Vertex::Red &&
                    NumberUtils::within((**it)[Vertex::Time], pos.time, pos.timeEnd()) &&
                    NumberUtils::within((**it)[Vertex::Blue], pos.blue, pos.blueEnd())) ||
                (dims.x == Vertex::Blue &&
                    NumberUtils::within((**it)[Vertex::Time], pos.time, pos.timeEnd()) &&
                    NumberUtils::within((**it)[Vertex::Red],  pos.red,  pos.redEnd())))
            {
                x = (**it)[dims.x];
                y = (**it)[dims.y];

                dist = (x - mx) * (x - mx) + (y - my) * (y - my);

                if(dist < radiusSqr)
                {
                    float weight = dist / radiusSqr;
                    (**it)[dims.x] += weight * weight * (mx - state.lastMouse.y);
                    (**it)[dims.y] += weight * weight * (my - state.lastMouse.y);

                    NumberUtils::constrain((**it)[dims.x], vertexLimits[dims.x]);
                    NumberUtils::constrain((**it)[dims.y], vertexLimits[dims.y]);
                }
            }
        }
        */

        // wtf is this??
    }
}

/*
void Interactor3D::beginAction(int action)
{
    state.start = state.currentMouse;
    vector<Vertex*>& selected = getSelected();

    String message = String();

    if (action == Actions::Cut)
    {
        if (getMesh()->cubes.empty())
            message = String("There are no lines to cut!");
        else
        {
            state.actionState = PanelState::Cutting;
            message = String("Press 'k' to end cut");
        }
    }
    else if (action == Actions::Rotate)
    {
        if (selected.size() < 2)
            message = String("Must select at least 2 vertices to rotate");
        else
        {
            state.actionState = PanelState::Rotating;

            updateSelectionFrames();
            message = String("Press 'r' to end rotate");
        }
    }
    else if (action == Actions::Stretch)
    {
        if (selected.size() < 2)
            message = String("Must select at least 2 vertices to stretch");
        else
        {
            state.actionState = PanelState::Stretching;

            updateSelectionFrames();
            message = String("Press 's' to end stretch");
        }
    }
    else if (action == Actions::Copy)
    {
        if (selected.size() < 2)
            message = String("Must select at least 1 vertex to duplicate");
        else
        {
            UndoableMeshProcess copyProcess(this);
            copyVertices();

            message =
                    (selected.size() == 1) ?
                            String("1 vertex copied") : String((int) selected.size()) + String(" vertices copied");
        }
    }
    else if (action == Actions::Extrude)
    {
        if (selected.size() < 1)
            message = String("Must select at least 1 vertex to extude");
        else
        {
            state.actionState = PanelState::Extruding;
            UndoableMeshProcess extrudeProcess(this);
            extrudeVertices();
        }
    }
    else if (action == Actions::Align)
    {
        if(selected.size() < 2)
            message = String("Must select at least 2 vertices to align");
        else
        {
            vector<Vertex> before = state.positions;
            updateSelectionFrames();
            Vertex2 pivot = state.getClosestPivot();

            if(ModifierKeys::getCurrentModifiers().isShiftDown())
            {
                for(vector<Vertex*>::iterator it = selected.begin(); it != selected.end(); ++it)
                    (**it)[dims.y] = pivot.y;
            }
            else
            {
                for(vector<Vertex*>::iterator it = selected.begin(); it != selected.end(); ++it)
                    (**it)[dims.x] = pivot.x;
            }

            copyVertexPositions();

            vector<Vertex> after = state.positions;
            TransformVerticesAction* alignAction = new TransformVerticesAction(this, &getMesh()->verts, before, after);
            getObj(EditWatcher).addAction(alignAction);
        }
    }

    if(message != String())
        showCritical(message);
}
*/

/*
void Interactor3D::endAction(int action)
{
    PanelState::ActionState& actionState = state.actionState;
    vector<Vertex*>& verts = getMesh()->verts;

    if(action == Actions::Cut)
    {
        actionState = PanelState::Cutting;

        UndoableMeshProcess sliceProcess(this);
        sliceLines();
    }
    else if(action == Actions::Rotate)
    {
        actionState = PanelState::Rotating;

        vector<Vertex> after;
        for(vector<Vertex*>::iterator it = verts.begin(); it != verts.end(); ++it)
        {
            if((**it)[dims.x] < 0)
                (**it)[dims.x] = 0;
            after.push_back(**it);
        }

        TransformVerticesAction* action = new TransformVerticesAction(this, &verts, state.positions, after);
        getObj(GlobalOperations).performAction(action);
    }
    else if(action == Actions::Stretch)
    {
        actionState = PanelState::Stretching;

        vector<Vertex> after;

        for(vector<Vertex*>::iterator it = verts.begin(); it != verts.end(); ++it)
            after.push_back(**it);

        TransformVerticesAction* action = new TransformVerticesAction(this, &verts, state.positions, after);
        getObj(GlobalOperations).performAction(action);
    }
    else if(action == Actions::Copy)
        actionState = PanelState::Copying;

    else if(action == Actions::Extrude)
        actionState = PanelState::Extruding;
}
*/

void Interactor3D::mergeVertices(
        Vertex* firstSelected,
        Vertex* secondSelected,
        MergeActionType mergeAction) {
    ScopedLock sl(vertexLock);

    Mesh* mesh = getMesh();
    vector<Vertex*>& verts = mesh->getVerts();

    if(! firstSelected || ! secondSelected) return;
    if(firstSelected == secondSelected)     return;

    int numOwnersA = firstSelected->getNumOwners();
    int numOwnersB = secondSelected->getNumOwners();

    if(numOwnersA == 0 && numOwnersB == 0) {
        Vertex* toRemove = mergeAction == MergeAtFirst ? secondSelected : firstSelected;

        removeFromVector(verts, toRemove);
    }

    // one of the verts is connected to a linecube
    else if((numOwnersA == 1 && numOwnersB == 0) || (numOwnersA == 0 && numOwnersB == 1)) {
        mergeVerticesOneOwner(firstSelected, secondSelected, mergeAction);
    } else if (numOwnersA == 1 && numOwnersB == 1) {
        mergeVerticesBothOneOwner(firstSelected, secondSelected, mergeAction);
    } else if ((numOwnersA > 1 && numOwnersB == 1) || (numOwnersA == 1 && numOwnersB > 1)) {
        mergeVerticesOneAndManyOwners(firstSelected, secondSelected, mergeAction);
    } else if (numOwnersA > 1 && numOwnersB > 1) {
        mergeVerticesManyOwners(firstSelected, secondSelected, mergeAction);
    }

    flag(DidMeshChange) = true;
}

void Interactor3D::mergeVerticesManyOwners(Vertex* firstSelected, Vertex* secondSelected, MergeActionType mergeAction) {
    VertCube* containsBoth = getLineContaining(firstSelected, secondSelected);
    Mesh* mesh = getMesh();

    Array<VertCube*> ownersA = firstSelected->owners;
    Array<VertCube*> ownersB = secondSelected->owners;

    ownersA.removeFirstMatchingValue(containsBoth);
    ownersB.removeFirstMatchingValue(containsBoth);

    for (auto& it : ownersA) {
        VertCube::Face face = it->getFace(dims.x, firstSelected);

        // TODO: remove verts from face
    }
/*
    if(mergeAction == MergeAtFirst || mergeAction == MergeAtSecond)
    {
        Array<Vertex*>& secondVector = mergeAction == MergeAtFirst ? connectedB : connectedA;

        foreach(Vertex**, it, secondVector)
            mesh->removeVert(*it);
    }
*/
    mesh->removeCube(containsBoth);
}

void Interactor3D::mergeVerticesOneAndManyOwners(Vertex* firstSelected, Vertex* secondSelected,
                                                 MergeActionType mergeAction) {
    Vertex* ownedByMany  = firstSelected->getNumOwners() > 1 ? firstSelected : secondSelected;
    Vertex* ownedByOne   = ownedByMany == firstSelected ? secondSelected : firstSelected;

    VertCube* commonCube = getLineContaining(ownedByMany, ownedByOne);
    VertCube* otherLine  = (commonCube == ownedByMany->owners[0]) ? ownedByMany->owners[1] : ownedByMany->owners[0];

    Mesh* mesh = getMesh();

    Array<Vertex*> notContained;
    for (int i = 0; i < VertCube::numVerts; ++i) {
        Vertex* vert = commonCube->getVertex(i);
        bool otherLineContains = otherLine->indexOf(vert) != CommonEnums::Null;

        if (! otherLineContains) {
            notContained.add(vert);
        }
    }

    for (auto& it : notContained) {
        mesh->removeVert(it);
    }

    mesh->removeCube(commonCube);
}

void Interactor3D::mergeVerticesBothOneOwner(
        Vertex* firstSelected,
        Vertex* secondSelected,
        MergeActionType mergeAction) {
    VertCube* ownsFirst = firstSelected->owners.getFirst();
    VertCube* ownsSecnd = secondSelected->owners.getFirst();

    Mesh* mesh = getMesh();

    // these two share a linecube
    if(ownsFirst == ownsSecnd) {
        for(auto& lineVert : ownsFirst->lineVerts) {
            mesh->removeVert(lineVert);
        }

        getObj(EditWatcher).addAction(new VertexOwnershipAction(ownsFirst, false, ownsFirst->toArray()), false);
        ownsFirst->orphanVerts();
        mesh->removeCube(ownsFirst);

        state.currentCube   = nullptr;
        state.currentVertex = nullptr;
    } else {
        // these get the verts in their dimension order
        VertCube::Face faceA = ownsFirst->getFace(dims.x, firstSelected);
        VertCube::Face faceB = ownsSecnd->getFace(dims.x, secondSelected);

        bool pole = ownsFirst->poleOf(dims.x, firstSelected);
        ownsFirst->setFace(faceB, dims.x, pole);

        for(int i = 0; i < faceA.size(); ++i) {
            mesh->removeVert(faceA[i]);
        }
    }
}

void Interactor3D::moveVertsAndTest(const Array<Vertex*>& arr, float diff) {
    for (auto it : arr) {
        it->values[dims.x] += diff;
    }

    collisionDetector.setCurrentSelection(getMesh(), arr);
    if(! collisionDetector.validate()) {
        for (auto it : arr) {
            it->values[dims.x] -= diff;
        }

        showConsoleMsg("Cannot merge due to line collision");
    }
}

void Interactor3D::mergeVerticesOneOwner(
        Vertex* firstSelected,
        Vertex* secondSelected,
        MergeActionType mergeAction) {
    Vertex* hasOneOwner     = firstSelected->getNumOwners() == 1 ? firstSelected : secondSelected;
    Vertex* hasZeroOwners   = hasOneOwner == firstSelected ? secondSelected : firstSelected;

    float diffBA    = secondSelected->values[dims.x] - firstSelected->values[dims.x];
    VertCube* cube  = hasOneOwner->owners.getFirst();

    if(mergeAction == MergeAtFirst) {
        // move b over to a's position
        if (hasOneOwner == secondSelected) {
            VertCube::Face face = cube->getFace(dims.x, hasOneOwner);
            moveVertsAndTest(face.toArray(), diffBA);
        }
    } else if (mergeAction == MergeAtSecond || mergeAction == MergeAtCentre) {
        float diff = diffBA;

        if (mergeAction == MergeAtCentre) {
            diff = diffBA * 0.5f;
        } else if(hasOneOwner != firstSelected) {
            diff = -diffBA;
        }

        moveVertsAndTest(cube->toArray(), diff);
    }

    getMesh()->removeVert(hasZeroOwners);
}


bool Interactor3D::connectVertices(Vertex* a, Vertex* b) {
    ScopedLock sl(vertexLock);

    // if both are unconnected, we don't need to worry about joining up the hyperverts

    int currentAxis     = getSetting(CurrentMorphAxis);
    MorphPosition pos   = positioner->getOffsetPosition(true);
    Mesh* mesh          = getMesh();

    vector<Vertex*> beforeVerts = mesh->getVerts();
    vector<VertCube*> beforeCubes = mesh->getCubes();

    vector<Vertex*> vertsToDeleteOnFailure;
    vector<Vertex*> vertsToRemoveOnSuccess;

    VertCube* addedCube = nullptr;

    if (a->unattached() && b->unattached()) {
        addedCube = new VertCube(mesh);
        addedCube->initVerts(pos);

        Vertex* poleVerts[] = { a, b };

        for(int p = 0; p < 2; ++p) {
            VertCube::Face face = addedCube->getFace(dims.x, p > 0);

            for (int i = 0; i < face.size(); ++i) {
                face[i]->values[dims.x] = poleVerts[p]->values[dims.x];
                face[i]->values[dims.y] = poleVerts[p]->values[dims.y];
            }
        }

        for(auto lineVert : addedCube->lineVerts)
            vertsToDeleteOnFailure.push_back(lineVert);

        vertsToRemoveOnSuccess.push_back(a);
        vertsToRemoveOnSuccess.push_back(b);
    }

    // one is unconnected, one already connected
    else if (a->unattached() || b->unattached()) {
        Vertex* freeVert      = a->unattached() ? a : b;
        Vertex* connectedVert = a->unattached() ? b : a;
        VertCube* cube        = nullptr;

        for(auto owner : connectedVert->owners) {
            int index = owner->indexOf(connectedVert);

            if(index != CommonEnums::Null) {
                cube = owner;
            }
        }

        jassert(cube != nullptr);
        if(cube == nullptr) {
            return false;
        }

        // set verts of the exisint cube's inner face to the new cube's inner face
        VertCube::Face cnxdFace = cube->getFace(currentAxis, connectedVert);

        bool polarity  = cube->poleOf(currentAxis, connectedVert);
        bool otherPole = ! polarity;

        addedCube = new VertCube();
        addedCube->setFace(cnxdFace, currentAxis, otherPole);

        // create new verts for the opposing face of the new cube
        Vertex diffValues = *freeVert - *connectedVert;
        VertCube::Face newFace(dims.x);

        for(int i = 0; i < cnxdFace.size(); ++i) {
            auto* vert = new Vertex(*cnxdFace[i] + diffValues);

            mesh->addVertex(vert);
            vertsToDeleteOnFailure.push_back(vert);
            newFace.set(i, vert);
        }

        addedCube->setFace(newFace, currentAxis, polarity);
        vertsToRemoveOnSuccess.push_back(freeVert);
    } else {
        VertCube* ownsA = a->owners.getFirst();
        VertCube* ownsB = b->owners.getFirst();

        VertCube::Face faceA = ownsA->getFace(currentAxis, a);
        VertCube::Face faceB = ownsB->getFace(currentAxis, b);

        bool polarityA = ownsA->poleOf(getSetting(CurrentMorphAxis), a);
        bool otherPole = ! polarityA;

        addedCube = new VertCube();
        addedCube->setFace(faceA, currentAxis, otherPole);
        addedCube->setFace(faceB, currentAxis, polarityA);
    }

    bool succeeded = false;

    for (int i = 0; i < VertCube::numVerts; ++i) {
        addedCube->getVertex(i)->addOwner(addedCube);
    }

    if (!suspendUndo) {
        succeeded = commitCubeAdditionIfValid(addedCube, beforeCubes, beforeVerts);
    }

    if (succeeded) {
        for (auto& vertsToRemoveOnSucces : vertsToRemoveOnSuccess) {
            mesh->removeVert(vertsToRemoveOnSucces);
        }

        flag(DidMeshChange) = true;
    } else {
        for (auto& it : vertsToDeleteOnFailure) {
            mesh->removeVert(it);
        }
    }

    // don't want dangling pointers to deleted verts
    clearSelectedAndCurrent();

    return succeeded;
}

bool Interactor3D::connectSelected() {
    bool succeeded = false;
    vector<Vertex*>& selected = getSelected();

    if (selected.size() == 2) {
        succeeded = connectVertices(selected[0], selected[1]);

        if (succeeded) {
            resetFinalSelection();
        }
    } else {
        showConsoleMsg("Select exactly two vertices to connect");
    }

    {
        ScopedLock sl(vertexLock);
        selected.clear();
        updateSelectionFrames();
    }

    flag(DidMeshChange) = succeeded;

    Interactor* opposite = getOppositeInteractor();
    if(opposite) {
        opposite->performUpdate(Update);
    }

    return succeeded;
}

void Interactor3D::copyVertices() {
    // TODO
    /*vector<Vertex*> copiedVerts;
    set<int> copiedIndices;
    int hash;

    for(vector<Vertex*>::iterator it = selected.begin(); it != selected.end(); ++it) {
        Vertex* copy = new Vertex();
        *copy = **it;

        copiedVerts.push_back(copy);
        getVerts()->push_back(copy);
    }

//  vector<VertCube*>& lines = getLines();
    for(int i = 0; i < (int) selected.size(); ++i) {
        VertCube* lines[] = { selected[i]->ownerA, selected[i]->ownerB };

        for(int k = 0; k < 2; k++) {
            for(int j = 0; j < selected.size(); ++j) {
                hash = 1000000 * jmax(i, j) + jmin(i, j);

                if(i != j && line->owns(selected[j]) && copiedIndices.find(hash) == copiedIndices.end()) {
                    copiedIndices.insert(hash);
                    (new VertCube(copiedVerts[i], copiedVerts[j], getMesh()));
                }

            }
        }
    }

    // put copied verts into selected vector
    selected.clear();
    copy(copiedVerts.begin(), copiedVerts.end(), inserter(selected, selected.end()));*/
}

void Interactor3D::extrudeVertices() {
    ScopedLock sl(vertexLock);

    UndoableMeshProcess extrudeProcess(this, "Extrude");

    vector<Vertex*>& selected = getSelected();

    // connectVertices() will clear selected
    vector<Vertex*> selectedCopy = selected;

    if(selectedCopy.empty()) {
        return;
    }

    vector<Vertex*> toSelect;

    // offset to make the duplicated vert just a little bit closer
    // to the mouse cursor so that it is selected over it's dupe on right-click drag

    for (auto vert : selectedCopy) {
        Vertex2 offset = (state.currentMouse - Vertex2(vert->values[dims.x], vert->values[dims.y])) * 0.02f;
        auto* fv = new Vertex(*vert);

        fv->values[dims.x] = vert->values[dims.x] + offset.x;
        fv->values[dims.y] = vert->values[dims.y] + offset.y;

        bool succeeded = connectVertices(vert, fv);

        if(succeeded) {
            toSelect.push_back(fv);
        }
    }

    selected.clear();
    selected = toSelect;

    updateSelectionFrames();
    resizeFinalBoxSelection(true);

    flag(DidMeshChange) = true;
}

// todo account for all dimensions
void Interactor3D::sliceLines(const Vertex2& start, const Vertex2& end) {
    ScopedLock sl(vertexLock);

    vector<VertCube*> toAdd, toRemove, newCubes;
    vector<Vertex*> newVerts;

    Mesh* mesh = getMesh();
    EditWatcher* editWatcher = &getObj(EditWatcher);
    MorphPosition pos = getOffsetPosition(true);

    for (auto* cube : mesh->getCubes()) {
        if(! cube->intersectsMorphRect(dims.x, reduceData, pos))
            continue;

        cube->getInterceptsFast(dims.x, reduceData, pos);

        SurfaceLine line(&reduceData.v0, &reduceData.v1, dims.x);
        Vertex2 point = line.getCrossPoint(start, end, dims.x, dims.y);

        if(point == Vertex2(-1, -1))
            continue;

        float xMax = reduceData.v0.values[dims.x];
        float xMin = reduceData.v1.values[dims.x];
        float prog = (point.x - xMin) / (xMax - xMin);

        NumberUtils::constrain(prog, 0.f, 1.f);

        editWatcher->addAction(new VertexOwnershipAction(cube, false, cube->toArray()), false);
        cube->orphanVerts();

        VertCube::Face newFace(dims.x);
        VertCube::Face lowFace  = cube->getFace(dims.x, VertCube::LowPole);
        VertCube::Face highFace = cube->getFace(dims.x, VertCube::HighPole);

        for (int i = 0; i < VertCube::numVerts / 2; ++i) {
            auto* vert = new Vertex(*lowFace[i] * (1 - prog) + *highFace[i] * prog);

            newFace.set(i, vert);
            mesh->addVertex(vert);
            newVerts.push_back(vert);
        }

        auto* lowCube  = new VertCube();
        auto* highCube = new VertCube();

        newCubes.push_back(lowCube);
        newCubes.push_back(highCube);

        transferLineProperties(cube, lowCube, highCube);

        lowCube->setFace(lowFace, dims.x, VertCube::LowPole);
        lowCube->setFace(newFace, dims.x, VertCube::HighPole);

        highCube->setFace(newFace, dims.x, VertCube::LowPole);
        highCube->setFace(highFace, dims.x, VertCube::HighPole);

        // do not add or delete lines from mesh immediately or this will invalidate iterators
        toAdd.push_back(lowCube);
        toAdd.push_back(highCube);
        toRemove.push_back(cube);
    }

    removeDuplicateVerts(newVerts, newCubes, true);

    vector<Vertex*>& selected = getSelected();
    selected.clear();

    for (auto& it : toAdd) {
        mesh->addCube(it);
        selected.push_back(it->findClosestVertex(pos));
    }

    // don't delete line from memory yet because undo might reclaim it
    for (auto& it : toRemove) {
        mesh->removeCube(it);
    }

    for (auto& it : mesh->getCubes()) {
        it->validate();
    }

    updateSelectionFrames();

    flag(DidMeshChange) = true;
}

void Interactor3D::removeDuplicateVerts(
        vector<Vertex*>& affectedVerts,
        vector<VertCube*>& affectedCubes,
        bool deleteDupes) {
    vector<Vertex*> duplicates;
    vector<Vertex*> originals;

    EditWatcher* editWatcher = &getObj(EditWatcher);
    Mesh* mesh = getMesh();

    for(auto v1 : affectedVerts) {
        for(auto v2 : affectedVerts) {
            if (v1 == v2) {
                continue;
            }

            if (*v1 == *v2) {
                if (std::find(duplicates.begin(), duplicates.end(), v1) == duplicates.end()) {
                    originals.push_back(v1);
                    duplicates.push_back(v2);
                }
            }
        }
    }

    for (auto& affectedCube : affectedCubes) {
        for (int i = 0; i < (int) duplicates.size(); ++i) {
            int index = affectedCube->indexOf(duplicates[i]);

            if (index != CommonEnums::Null) {
                VertCube* cube = affectedCube;
                cube->setVertex(originals[i], index);
                editWatcher->addAction(new VertexOwnershipAction(cube, true, Array<Vertex*>(&originals[i], 1)), false);
            }
        }
    }

    if (deleteDupes) {
        for (auto& duplicate : duplicates) {
            mesh->removeVert(duplicate);
            delete duplicate;
        }
    }

    mesh->validate();
    mesh->removeFreeVerts();
}


void Interactor3D::glueCubes() {
    UndoableMeshProcess process(this, "Glue Cubes");

    if (isPositiveAndBelow(state.currentIcpt, (int) interceptPairs.size())) {
        SimpleIcpt& icpt = interceptPairs[state.currentIcpt];

        // we cannot glue an intercept
        if (icpt.pole == false) {
            Mesh* mesh = getMesh();

            VertCube* startingCube = icpt.parent;
            MorphPosition pos = positioner->getOffsetPosition(true);

            Array<VertCube*> currDivision;
            currDivision = startingCube->getAllAdjacentCubes(dims.x, pos);

            for (auto cube : currDivision) {
                VertCube::Face face1 = (cube)->getFace(dims.x, VertCube::LowPole);
                VertCube::Face face2 = (cube)->getFace(dims.x, VertCube::HighPole);

                // dedupe step will remove unneeded verts
                for(int i = 0; i < face1.size(); ++i)
                    *face1[i] = *face2[i];

                if (VertCube* behind = cube->getAdjacentCube(dims.x, VertCube::LowPole)) {
                    VertCube::Face lowFace = behind->getFace(dims.x, VertCube::HighPole);
                }

                mesh->removeCube(cube);
            }

            removeDuplicateVerts(mesh->getVerts(), mesh->getCubes(), false);

            flag(DidMeshChange) = true;
        }
    }
}

void Interactor3D::glueCubes(VertCube* cubeOne, VertCube* cubeTwo) {
    // TODO
}

void Interactor3D::commitPath(const MouseEvent& e) {
    ScopedLock sl(vertexLock);

    Mesh* mesh = getMesh();

    if(actionIs(PaintingEdit)) {
        doCommitPencilEditPath();
    }

    else if(actionIs(PaintingCreate))
    {
        if(pencilPath.empty()) {
            return;
        }

        MorphPosition pos = getModPosition();
        vector<Vertex*> addedVerts;

        {
            ScopedValueSetter setter(suspendUndo, true);
            UndoableMeshProcess commitPathProcess(this, "Pencil Path Created");

            for (auto & it : pencilPath) {
                auto* vert = new Vertex();

                vert->values[Vertex::Time] = pos.time;
                vert->values[Vertex::Red]  = pos.red;
                vert->values[Vertex::Blue] = pos.blue;
                vert->values[Vertex::Amp]  = 0.5f;

                //will overwrite whichever axis we're on
                vert->values[dims.y] = it.y;
                vert->values[dims.x] = it.x;

                addedVerts.push_back(vert);
                mesh->addVertex(vert);
            }

            for(int i = 0; i < (int) addedVerts.size() - 1; ++i) {
                connectVertices(addedVerts[i], addedVerts[i + 1]);
            }
        }

        flag(DidMeshChange) = true;
    }
}

VertCube* Interactor3D::getLineContaining(Vertex* a, Vertex* b) {
    if(a == b) {
        return nullptr;
    }

    VertCube* cube = nullptr;
    Mesh* mesh = getMesh();

    for (auto& it : mesh->getCubes()) {
        bool matchesA = false, matchesB = false;
        for (int i = 0; i < VertCube::numVerts; ++i) {
            Vertex* vert = it->getVertex(i);

            if (a == vert) {
                matchesA = true;
            }

            if (b == vert) {
                matchesB = true;
            }
        }

        if (matchesA && matchesB) {
            cube = it;
            break;
        }
    }

    return cube;
}

void Interactor3D::mergeSelectedVerts() {
    vector<Vertex*>& selected = getSelected();

    if (selected.size() == 2) {
        mergeVertices(selected[0], selected[1], MergeAtCentre);
    } else {
        showConsoleMsg("Must select two vertices to merge");
    }
}

bool Interactor3D::doCreateVertex() {
    ScopedLock sl(vertexLock);

    vector<Vertex*>& selected = getSelected();
    selected.clear();

    bool succeeded = addNewCube(state.currentMouse.x, state.currentMouse.y, 0.5f, 0.f);

    resetFinalSelection();
    setMovingVertsFromSelected();

    return succeeded;
}

void Interactor3D::doAxe(const MouseEvent& e) {
    ScopedLock sl(vertexLock);

    Vertex2 start   = state.currentMouse + Vertex2(0, realValue(PencilRadius));
    Vertex2 end     = state.currentMouse - Vertex2(0, realValue(PencilRadius));

    if(! getMesh()) {
        return;
    }

    {
        UndoableMeshProcess axeProcess(this, "Axe");

        sliceLines(start, end);
    }

    state.actionState = PanelState::DraggingVertex;

    flag(DidMeshChange) = true;
}

float Interactor3D::getAverageDistanceFromCentre() {
    vector<Vertex*>& selected = getSelected();

    float sum = 0;
    float diffX, diffY;

    for (auto vert : selected) {
        diffX = vert->values[dims.x] - state.pivots[PanelState::CentrePivot].x;
        diffY = vert->values[dims.y] - state.pivots[PanelState::CentrePivot].y;
        sum += diffX * diffX + diffY * diffY;
    }

    return sqrtf(sum / float(selected.size()));
}

void Interactor3D::primaryDimensionChanged() {
    int newDim = getSetting(CurrentMorphAxis);
    auto it = std::find(dims.hidden.begin(), dims.hidden.end(), newDim);

    // this interactor had the new dim hidden
    // exchange the currnet x-dim with the new one
    if (it != dims.hidden.end()) {
        int xDimCopy = dims.x;
        dims.x = newDim;

        dims.hidden.erase(it);
        dims.hidden.push_back(xDimCopy);
    }

    updateRastDims();
}

void Interactor3D::updateRastDims() {
    progressMark

    rasterizer->setDims(dims);
}

void Interactor3D::updateInterceptsWithMesh(Mesh* mesh) {
    ScopedLock sl(vertexLock);
    interceptPairs.clear();

    MorphPosition pos = getModPosition();

    bool wrapsVerts = getRasterizer()->wrapsVertices();

    if(mesh == nullptr) {
        return;
    }

    for (auto& lit : mesh->getCubes()) {
        lit->getInterceptsFast(dims.x, reduceData, pos);

        if(! reduceData.lineOverlaps) {
            continue;
        }

        float ax = reduceData.v0.values[dims.x];
        float ay = reduceData.v0.values[dims.y];
        float bx = reduceData.v1.values[dims.x];
        float by = reduceData.v1.values[dims.y];

        if (wrapsVerts) {
            if ((ay > 1) != (by > 1)) {
                interceptPairs.emplace_back(ax, ay - 1, lit, false, true);
                interceptPairs.emplace_back(bx, by - 1, lit, true, true);
            } else if (ay > 1 && by > 1) {
                // phase validation should have reset this
                ay -= 1.f;
                by -= 1.f;
            }
        } else {
            NumberUtils::constrain(ay, vertexLimits[dims.y]);
            NumberUtils::constrain(by, vertexLimits[dims.y]);
        }

        interceptPairs.emplace_back(ax, ay, lit, false);
        interceptPairs.emplace_back(bx, by, lit, true);
    }
}

void Interactor3D::sliceLines() {
    sliceLines(state.start, state.currentMouse);
}

void Interactor3D::doExtraMouseDrag(const MouseEvent& e) {
}

void Interactor3D::updateIntercepts() {
    Mesh* mesh = getMesh();

    updateInterceptsWithMesh(mesh);
}

void Interactor3D::shallowUpdate() {
    updateIntercepts();
    Interactor::performUpdate(Update);
}

int Interactor3D::getTableIndexY(float y, int size) {
    return jlimit(0, size - 1, int(y * (size - 1)));
}

void Interactor3D::doFurtherReset() {
    ScopedLock sl(vertexLock);

    interceptPairs.clear();
}

void Interactor3D::doExtraMouseUp() {
}

void Interactor3D::doCommitPencilEditPath() {
}

void Interactor3D::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        updateIntercepts();
        panel->bakeTexturesNextRepaint();
    }

    Interactor::performUpdate(updateType);
}

String Interactor3D::getZString(float val, int xIdx) {
    return {val * 2, 3};
}

String Interactor3D::getYString(
        float yVal,
        int yIndex,
        const Column& col,
        float fundFreq) {
    return {yVal, 3};
}

