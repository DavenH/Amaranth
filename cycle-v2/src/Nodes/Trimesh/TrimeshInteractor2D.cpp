#include "TrimeshInteractor2D.h"

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Definitions.h>

namespace CycleV2 {

TrimeshInteractor2D::TrimeshInteractor2D(
        SingletonRepo* repo,
        const String& name,
        const Dimensions& dimensions) :
        SingletonAccessor(repo, name)
    ,   Interactor2D(repo, name, dimensions) {}

void TrimeshInteractor2D::setMeshEditedCallback(std::function<void()> callback) {
    meshEditedCallback = std::move(callback);
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
        meshEditedCallback();
    }
}

}
