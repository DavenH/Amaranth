#include "TrimeshInteractor3D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Curve/Mesh/VertCube.h>
#include <Definitions.h>
#include <UI/Panels/Panel.h>

#include <algorithm>
#include <limits>

namespace CycleV2 {

namespace {

bool hasCompleteLineVertices(const VertCube& cube) {
    for (Vertex* vertex : cube.lineVerts) {
        if (vertex == nullptr) {
            return false;
        }
    }

    return true;
}

bool validDistance(float distance) {
    return distance == distance;
}

}

TrimeshInteractor3D::TrimeshInteractor3D(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name)
    ,   Interactor3D(repo, name) {}

void TrimeshInteractor3D::setMeshEditedCallback(std::function<void(bool)> callback) {
    meshEditedCallback = std::move(callback);
}

bool TrimeshInteractor3D::locateClosestElement() {
    ScopedLock sl(vertexLock);

    if (display == nullptr || panel == nullptr || display->getWidth() <= 0 || display->getHeight() <= 0) {
        state.currentIcpt = -1;
        state.currentFreeVert = -1;
        state.currentCube = nullptr;
        state.currentVertex = nullptr;
        return false;
    }

    depthVerts.erase(
            std::remove_if(
                    depthVerts.begin(),
                    depthVerts.end(),
                    [] (const DepthVert& vertex) {
                        return vertex.vert == nullptr;
                    }),
            depthVerts.end());

    interceptPairs.erase(
            std::remove_if(
                    interceptPairs.begin(),
                    interceptPairs.end(),
                    [] (const SimpleIcpt& intercept) {
                        return intercept.parent == nullptr || !hasCompleteLineVertices(*intercept.parent);
                    }),
            interceptPairs.end());

    if (depthVerts.empty() && interceptPairs.empty()) {
        state.currentIcpt = -1;
        state.currentFreeVert = -1;
        state.currentCube = nullptr;
        state.currentVertex = nullptr;
        return false;
    }

    const int oldIcpt = state.currentIcpt;
    const int oldFreeVert = state.currentFreeVert;
    const float invWidth = 1.f / (float) display->getWidth();
    const float invHeight = 1.f / (float) display->getHeight();
    const float sxPos = panel->sx(state.currentMouse.x);
    const float syPos = panel->sy(state.currentMouse.y);

    float minDist = std::numeric_limits<float>::max();
    int closestDepthVertIdx = -1;
    int closestIcptIdx = -1;
    bool depthIsClosest = true;

    for (int i = 0; i < (int) depthVerts.size(); ++i) {
        const DepthVert& depthVert = depthVerts[(size_t) i];
        const float diffX = (panel->sx(depthVert.x) - sxPos) * invWidth;
        const float diffY = (panel->sy(depthVert.y) - syPos) * invHeight;
        const float dist = diffX * diffX + diffY * diffY;

        if (validDistance(dist) && dist < minDist) {
            minDist = dist;
            closestDepthVertIdx = i;
        }
    }

    for (int i = 0; i < (int) interceptPairs.size(); ++i) {
        const SimpleIcpt& intercept = interceptPairs[(size_t) i];
        const float diffX = (panel->sx(intercept.x) - sxPos) * invWidth;
        const float diffY = (panel->sy(intercept.y) - syPos) * invHeight;
        const float dist = diffX * diffX + diffY * diffY;

        if (validDistance(dist) && dist < minDist) {
            depthIsClosest = false;
            minDist = dist;
            closestIcptIdx = i;
        }
    }

    if (depthIsClosest && isPositiveAndBelow(closestDepthVertIdx, (int) depthVerts.size())) {
        state.currentCube = nullptr;
        state.currentVertex = depthVerts[(size_t) closestDepthVertIdx].vert;
        state.currentFreeVert = closestDepthVertIdx;
        state.currentIcpt = -1;

        const bool changed = oldFreeVert != state.currentFreeVert;
        flag(SimpleRepaint) |= changed;
        return changed;
    }

    if (!depthIsClosest && isPositiveAndBelow(closestIcptIdx, (int) interceptPairs.size())) {
        SimpleIcpt& icpt = interceptPairs[(size_t) closestIcptIdx];
        Vertex2 mousePos = state.currentMouse;

        if (icpt.isWrapped) {
            mousePos.y += 1.f;
        }

        state.currentCube = icpt.parent;
        state.currentIcpt = closestIcptIdx;
        state.currentFreeVert = -1;
        state.currentVertex = findLinesClosestVertex(state.currentCube, mousePos);

        const bool changed = oldIcpt != state.currentIcpt;
        flag(SimpleRepaint) |= changed;
        return changed;
    }

    state.currentIcpt = -1;
    state.currentFreeVert = -1;
    state.currentCube = nullptr;
    state.currentVertex = nullptr;
    return false;
}

void TrimeshInteractor3D::mouseDrag(const MouseEvent& event) {
    Interactor3D::mouseDrag(event);

    if (flag(DidMeshChange) && meshEditedCallback != nullptr) {
        meshEditedCallback(true);
    }
}

void TrimeshInteractor3D::mouseUp(const MouseEvent& event) {
    const bool meshChanged = flag(DidMeshChange);

    if (actionIs(BoxSelecting) || actionIs(ClickSelecting) || actionIs(DraggingCorner)) {
        vector<Vertex*>& selected = getSelected();

        if (!selected.empty()) {
            setMovingVertsFromSelected();
            listeners.call(&InteractorListener::focusGained, this);
            resizeFinalBoxSelection();
        }

        flag(SimpleRepaint) = true;
    }

    if (getSetting(Tool) == Tools::Pencil) {
        doMouseUpPencil(event);
    }

    selection.setBounds(0, 0, 0, 0);
    doExtraMouseUp();
    mouseFlag(FirstMove) = true;

    if (flag(DidMeshChange)) {
        performUpdate(Update);
    }

    refresh();
    flag(LoweredRes) = false;
    flag(SimpleRepaint) = false;

    if (meshChanged && meshEditedCallback != nullptr) {
        meshEditedCallback(true);
    }
}

}
