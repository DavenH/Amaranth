#include "TrimeshInteractor2D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Curve/Mesh/VertCube.h>
#include <Definitions.h>

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

}

TrimeshInteractor2D::TrimeshInteractor2D(
        SingletonRepo* repo,
        const String& name,
        const Dimensions& dimensions) :
        SingletonAccessor(repo, name)
    ,   Interactor2D(repo, name, dimensions) {}

void TrimeshInteractor2D::setMeshEditedCallback(std::function<void(bool)> callback) {
    meshEditedCallback = std::move(callback);
}

void TrimeshInteractor2D::setExtraElements(float) {
    ScopedLock sl(vertexLock);

    if (state.currentCube == nullptr) {
        return;
    }

    if (!hasCompleteLineVertices(*state.currentCube)) {
        return;
    }

    if (Vertex* closest = findLinesClosestVertex(state.currentCube, state.currentMouse)) {
        state.currentVertex = closest;
    }
}

void TrimeshInteractor2D::mouseDrag(const MouseEvent& event) {
    Interactor2D::mouseDrag(event);

    if (flag(DidMeshChange) && meshEditedCallback != nullptr) {
        meshEditedCallback(false);
    }
}

void TrimeshInteractor2D::mouseUp(const MouseEvent& event) {
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
        meshEditedCallback(false);
    }
}

}
