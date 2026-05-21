#include "TrimeshInteractor3D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Definitions.h>

namespace CycleV2 {

TrimeshInteractor3D::TrimeshInteractor3D(SingletonRepo* repo, const String& name) :
        SingletonAccessor(repo, name)
    ,   Interactor3D(repo, name) {}

void TrimeshInteractor3D::setMeshEditedCallback(std::function<void(bool)> callback) {
    meshEditedCallback = std::move(callback);
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
