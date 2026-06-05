#include "TrimeshPanel3D.h"

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true)
    ,   dataSource         (source) {
    applyGradientForDomain();
    volumeScale = 1.f;
    volumeTrans = 0.f;
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

void TrimeshPanel3D::setDisplayDomain(PortDomain domain) {
    if (displayDomain == domain) {
        return;
    }

    displayDomain = domain;
    renderProfile = TrimeshRenderProfile::fromDomain(domain);
    applyGradientForDomain();
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
}

void TrimeshPanel3D::applyGradientForDomain() {
    Image image = renderProfile.gradientImage();

    isTransparent = renderProfile.surfaceTextureUsesAlpha();
    gradient.read(image, true, isTransparent);
}

}
