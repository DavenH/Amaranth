#include "TrimeshPanel3D.h"

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true)
    ,   dataSource         (source) {
    applyGradientForProfile();
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
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshPanel3D::setRenderProfile(TrimeshRenderProfile profile) {
    if (profile.getDomain() == renderProfile.getDomain()
            && profile.getScalePolicy() == renderProfile.getScalePolicy()) {
        return;
    }

    renderProfile = profile;
    applyGradientForProfile();
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
}

void TrimeshPanel3D::applyGradientForProfile() {
    const auto& surfaceStyle = renderProfile.getSurfaceStyle();
    Image image = surfaceStyle.gradientImage();

    isTransparent = surfaceStyle.textureUsesAlpha;
    gradient.read(image, true, isTransparent);
}

}
