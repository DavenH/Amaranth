#include "NodeCanvasViewport.h"

namespace CycleV2 {

void NodeCanvasViewport::setBounds(juce::Rectangle<float> boundsToUse) {
    if (bounds == boundsToUse) {
        return;
    }

    bounds = boundsToUse;
    ++revision;
}

void NodeCanvasViewport::setTransform(juce::Point<float> panToUse, float zoomToUse) {
    const float constrainedZoom = juce::jlimit(minimumZoom, maximumZoom, zoomToUse);
    if (pan == panToUse && zoom == constrainedZoom) {
        return;
    }

    pan = panToUse;
    zoom = constrainedZoom;
    ++revision;
}

void NodeCanvasViewport::panBy(juce::Point<float> delta) {
    setTransform(pan + delta, zoom);
}

void NodeCanvasViewport::zoomAround(juce::Point<float> screenAnchor, float scaleFactor) {
    const juce::Point<float> anchorWorld = toWorld(screenAnchor);
    const float nextZoom = juce::jlimit(minimumZoom, maximumZoom, zoom * scaleFactor);
    const juce::Point<float> nextPan = screenAnchor - anchorWorld * nextZoom;
    setTransform(nextPan, nextZoom);
}

juce::Point<float> NodeCanvasViewport::toScreen(juce::Point<float> world) const {
    return pan + world * zoom;
}

juce::Point<float> NodeCanvasViewport::toWorld(juce::Point<float> screen) const {
    return (screen - pan) / zoom;
}

juce::Rectangle<float> NodeCanvasViewport::toScreen(juce::Rectangle<float> world) const {
    return {
            pan.x + world.getX() * zoom,
            pan.y + world.getY() * zoom,
            world.getWidth() * zoom,
            world.getHeight() * zoom
    };
}

juce::Rectangle<float> NodeCanvasViewport::toWorld(juce::Rectangle<float> screen) const {
    return {
            (screen.getX() - pan.x) / zoom,
            (screen.getY() - pan.y) / zoom,
            screen.getWidth() / zoom,
            screen.getHeight() / zoom
    };
}

juce::Point<float> NodeCanvasViewport::centreWorld() const {
    return toWorld(bounds.getCentre());
}

juce::Point<float> NodeCanvasViewport::snap(juce::Point<float> world, float interval) const {
    if (interval <= 0.f) {
        return world;
    }

    return {
            std::round(world.x / interval) * interval,
            std::round(world.y / interval) * interval
    };
}

}
