#include <algorithm>

#include "UI/NodeCanvasCableLayerCache.h"

namespace CycleV2 {

namespace {

bool cableStyleEqual(const NodeCableStyle& first, const NodeCableStyle& second) {
    return first.colour == second.colour
            && first.invalid == second.invalid
            && first.selected == second.selected
            && first.spliceTarget == second.spliceTarget
            && first.modulationBundle == second.modulationBundle;
}

}

bool NodeCanvasCableLayerCache::Entry::matches(
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        Rectangle<int> bounds,
        float currentZoom,
        float scale) const {
    return source == edge.source
            && destination == edge.destination
            && cablePath == edge.cablePath
            && destinationPortLike == edge.destinationPortLike
            && modulationBundle == edge.modulationBundle
            && destinationBundleIncludesYellow == edge.destinationBundleIncludesYellow
            && cableStyleEqual(styleSnapshot, style)
            && logicalBounds == bounds
            && zoom == currentZoom
            && physicalScale == scale
            && image.isValid();
}

void NodeCanvasCableLayerCache::beginFrame() {
    ++paintGeneration;
    frameStats = {};
}

NodeCanvasCableLayerCacheAccess NodeCanvasCableLayerCache::access(
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        Rectangle<int> logicalBounds,
        float zoom,
        float physicalScale) {
    auto match = std::find_if(
            entries.begin(),
            entries.end(),
            [&](const Entry& entry) { return entry.edgeIndex == edge.edgeIndex; });
    Entry* entry = match != entries.end() ? &*match : nullptr;
    const bool hit = entry != nullptr
            && entry->matches(edge, style, logicalBounds, zoom, physicalScale);
    if (!hit) {
        if (entry == nullptr) {
            entries.push_back({});
            entry = &entries.back();
        }
        replaceEntry(*entry, edge, style, logicalBounds, zoom, physicalScale);
        ++frameStats.misses;
    } else {
        ++frameStats.hits;
    }
    entry->paintGeneration = paintGeneration;
    return { &entry->image, logicalBounds, hit };
}

void NodeCanvasCableLayerCache::replaceEntry(
        Entry& entry,
        const NodeSceneEdge& sceneEdge,
        const NodeCableStyle& style,
        Rectangle<int> logicalBounds,
        float zoom,
        float physicalScale) {
    const int imageWidth = jmax(1, roundToInt(logicalBounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(logicalBounds.getHeight() * physicalScale));
    entry.edgeIndex = sceneEdge.edgeIndex;
    entry.source = sceneEdge.source;
    entry.destination = sceneEdge.destination;
    entry.cablePath = sceneEdge.cablePath;
    entry.styleSnapshot = style;
    entry.logicalBounds = logicalBounds;
    entry.zoom = zoom;
    entry.physicalScale = physicalScale;
    entry.destinationPortLike = sceneEdge.destinationPortLike;
    entry.modulationBundle = sceneEdge.modulationBundle;
    entry.destinationBundleIncludesYellow = sceneEdge.destinationBundleIncludesYellow;
    entry.image = Image(Image::ARGB, imageWidth, imageHeight, true);
}

void NodeCanvasCableLayerCache::draw(
        Graphics& graphics,
        const NodeCanvasCableLayerCacheAccess& access) const {
    jassert(access.image != nullptr && access.image->isValid());
    if (access.image == nullptr || !access.image->isValid()) {
        return;
    }
    const float imageToLogicalX = (float) access.logicalBounds.getWidth()
            / (float) access.image->getWidth();
    const float imageToLogicalY = (float) access.logicalBounds.getHeight()
            / (float) access.image->getHeight();
    graphics.drawImageTransformed(
            *access.image,
            AffineTransform(
                    imageToLogicalX,
                    0.f,
                    (float) access.logicalBounds.getX(),
                    0.f,
                    imageToLogicalY,
                    (float) access.logicalBounds.getY()),
            false);
}

NodeCanvasCableLayerCacheStats NodeCanvasCableLayerCache::endFrame() {
    entries.erase(
            std::remove_if(
                    entries.begin(),
                    entries.end(),
                    [&](const Entry& entry) { return entry.paintGeneration != paintGeneration; }),
            entries.end());
    return frameStats;
}

}
