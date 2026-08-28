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

void NodeCanvasCableLayerCache::beginFrame(
        Rectangle<int> visibleBounds,
        float physicalScale) {
    ++paintGeneration;
    frameEntryIndices.clear();
    frameVisibleBounds = visibleBounds;
    framePhysicalScale = physicalScale;
    frameStats = {};
}

NodeCanvasCableLayerCacheAccess NodeCanvasCableLayerCache::access(
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        Rectangle<int> logicalBounds,
        float zoom,
        float physicalScale) {
    jassert(physicalScale == framePhysicalScale);
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
    frameEntryIndices.push_back(static_cast<size_t>(entry - entries.data()));
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

void NodeCanvasCableLayerCache::rebuildComposite(Rectangle<int> bounds) {
    compositeBounds = bounds;
    compositePhysicalScale = framePhysicalScale;
    compositeEdgeIndices.clear();
    compositeEdgeIndices.reserve(frameEntryIndices.size());
    for (const size_t entryIndex : frameEntryIndices) {
        compositeEdgeIndices.push_back(entries[entryIndex].edgeIndex);
    }

    if (bounds.isEmpty()) {
        compositeImage = {};
        compositeInitialized = true;
        return;
    }

    const int imageWidth = jmax(1, roundToInt(bounds.getWidth() * framePhysicalScale));
    const int imageHeight = jmax(1, roundToInt(bounds.getHeight() * framePhysicalScale));
    compositeImage = Image(Image::ARGB, imageWidth, imageHeight, true);
    Graphics imageGraphics(compositeImage);
    imageGraphics.addTransform(AffineTransform(
            framePhysicalScale,
            0.f,
            -bounds.getX() * framePhysicalScale,
            0.f,
            framePhysicalScale,
            -bounds.getY() * framePhysicalScale));
    for (const size_t entryIndex : frameEntryIndices) {
        drawEntry(imageGraphics, entries[entryIndex]);
    }
    compositeInitialized = true;
}

void NodeCanvasCableLayerCache::drawEntry(Graphics& graphics, const Entry& entry) const {
    const float imageToLogicalX = (float) entry.logicalBounds.getWidth()
            / (float) entry.image.getWidth();
    const float imageToLogicalY = (float) entry.logicalBounds.getHeight()
            / (float) entry.image.getHeight();
    graphics.drawImageTransformed(
            entry.image,
            AffineTransform(
                    imageToLogicalX,
                    0.f,
                    (float) entry.logicalBounds.getX(),
                    0.f,
                    imageToLogicalY,
                    (float) entry.logicalBounds.getY()),
            false);
}

Rectangle<int> NodeCanvasCableLayerCache::frameCompositeBounds() const {
    Rectangle<int> bounds;
    for (const size_t entryIndex : frameEntryIndices) {
        const Entry& entry = entries[entryIndex];
        bounds = bounds.isEmpty()
                ? entry.logicalBounds
                : bounds.getUnion(entry.logicalBounds);
    }
    return bounds.getIntersection(frameVisibleBounds);
}

bool NodeCanvasCableLayerCache::compositeMatches(Rectangle<int> bounds) const {
    if (frameStats.misses != 0
            || !compositeInitialized
            || compositeBounds != bounds
            || compositePhysicalScale != framePhysicalScale
            || compositeEdgeIndices.size() != frameEntryIndices.size()) {
        return false;
    }
    for (size_t index = 0; index < frameEntryIndices.size(); ++index) {
        if (entries[frameEntryIndices[index]].edgeIndex != compositeEdgeIndices[index]) {
            return false;
        }
    }
    return true;
}

NodeCanvasCableLayerCacheFrame NodeCanvasCableLayerCache::endFrame() {
    const Rectangle<int> bounds = frameCompositeBounds();
    const bool compositeHit = compositeMatches(bounds);
    if (!compositeHit) {
        rebuildComposite(bounds);
    }

    entries.erase(
            std::remove_if(
                    entries.begin(),
                    entries.end(),
                    [&](const Entry& entry) { return entry.paintGeneration != paintGeneration; }),
            entries.end());
    return { frameStats, &compositeImage, compositeBounds, compositeHit };
}

void NodeCanvasCableLayerCache::drawComposite(
        Graphics& graphics,
        const NodeCanvasCableLayerCacheFrame& frame) const {
    if (frame.compositeBounds.isEmpty()) {
        return;
    }
    jassert(frame.compositeImage != nullptr && frame.compositeImage->isValid());
    if (frame.compositeImage == nullptr || !frame.compositeImage->isValid()) {
        return;
    }

    const float imageToLogicalX = (float) frame.compositeBounds.getWidth()
            / (float) frame.compositeImage->getWidth();
    const float imageToLogicalY = (float) frame.compositeBounds.getHeight()
            / (float) frame.compositeImage->getHeight();
    graphics.drawImageTransformed(
            *frame.compositeImage,
            AffineTransform(
                    imageToLogicalX,
                    0.f,
                    (float) frame.compositeBounds.getX(),
                    0.f,
                    imageToLogicalY,
                    (float) frame.compositeBounds.getY()),
            false);
}

}
