#include "TrimeshInteractor2D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Curve/Mesh/VertCube.h>
#include <Definitions.h>
#include <UI/Panels/Panel.h>

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

void TrimeshInteractor2D::setMeshEditedCallback(
        std::function<void(TrimeshMeshEditEvent)> callback) {
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

bool TrimeshInteractor2D::isCurrentVertexHit(Point<int> mousePosition) {
    if (panel == nullptr || state.currentCube == nullptr) {
        return false;
    }

    VertCube::ReductionData intercept;
    state.currentCube->getFinalIntercept(intercept, getMorphPosition());
    if (!intercept.pointOverlaps) {
        return false;
    }

    const Vertex2 pixelPosition(
            panel->sx(intercept.v.values[dims.x]),
            panel->sy(intercept.v.values[dims.y]));
    return pixelPosition.dist2(Vertex2(mousePosition.x, mousePosition.y)) <= 64.f;
}

bool TrimeshInteractor2D::doCreateVertex() {
    const bool created = Interactor2D::doCreateVertex();
    if (created && meshEditedCallback != nullptr) {
        meshEditedCallback({ false, false });
    }
    return created;
}

void TrimeshInteractor2D::mouseDrag(const MouseEvent& event) {
    Interactor2D::mouseDrag(event);

    if (flag(DidMeshChange) && meshEditedCallback != nullptr) {
        meshEditedCallback({ false, false });
    }
}

void TrimeshInteractor2D::mouseUp(const MouseEvent& event) {
    const bool meshChanged = flag(DidMeshChange);
    const bool createdVertex = actionIs(CreatingVertex);

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

    if ((meshChanged || createdVertex) && meshEditedCallback != nullptr) {
        meshEditedCallback({ false, true });
    }
}

void TrimeshInteractor2D::deleteSelected() {
    if (getSelected().empty()) {
        return;
    }
    eraseSelected();
    performUpdate(Update);
    if (meshEditedCallback != nullptr) {
        meshEditedCallback({ false, true });
    }
}

}
