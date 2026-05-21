#include "TrimeshPanel3D.h"

#include <Binary/Gradients.h>

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true)
    ,   dataSource         (source) {
    Image blue = PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);
    gradient.read(blue, true, false);
    volumeScale = 0.48f;
    volumeTrans = 0.02f;
    guideCurveApplicable = false;
    speedApplicable = false;
    pendingDeformUpdate = false;
}

void TrimeshPanel3D::panelResized() {
    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    updateNameTexturePos();
    updateBackground();
    doExtraResized();
}

}
