#include "CurvePanelInfrastructure.h"

namespace CycleV2 {

void CurvePanelSnapshotCache::publish(Image nextImage, bool hasVisibleContent) {
    const ScopedLock scopedLock(lock);
    image = std::move(nextImage);
    visibleContent = hasVisibleContent;
}

bool CurvePanelSnapshotCache::paint(
        Graphics& graphics,
        Rectangle<float> bounds,
        bool resample) const {
    const ScopedLock scopedLock(lock);
    if (!image.isValid() || !visibleContent) {
        return false;
    }

    Graphics::ScopedSaveState state(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());
    if (resample) {
        graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    }
    graphics.drawImage(image, bounds);
    return true;
}

}
