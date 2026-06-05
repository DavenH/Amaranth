#include "TrimeshPanel3D.h"

#include <Binary/Gradients.h>

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
    applyGradientForDomain();
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
}

void TrimeshPanel3D::applyGradientForDomain() {
    const bool spectral = displayDomain == PortDomain::SpectralMagnitudeSignal
            || displayDomain == PortDomain::SpectralPhaseSignal;
    Image image = spectral
            ? PNGImageFormat::loadFrom(Gradients::burntalum_png, Gradients::burntalum_pngSize)
            : PNGImageFormat::loadFrom(Gradients::blue_png, Gradients::blue_pngSize);

    gradient.read(image, true, false);
}

}
