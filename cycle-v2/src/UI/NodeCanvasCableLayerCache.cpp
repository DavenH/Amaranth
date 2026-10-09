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

uint64_t addGeometryValue(uint64_t hash, float value) {
    constexpr float quantization = 4096.f;
    hash ^= (uint32_t) roundToInt(value * quantization);
    return hash * 1099511628211ull;
}

uint64_t cableGeometryFingerprint(const NodeSceneEdge& edge) {
    uint64_t hash = 1469598103934665603ull;
    auto addPoint = [&](Point<float> point) {
        const Point<float> local = point - edge.source;
        hash = addGeometryValue(hash, local.x);
        hash = addGeometryValue(hash, local.y);
    };

    addPoint(edge.source);
    addPoint(edge.destination);
    Path::Iterator iterator(edge.cablePath);
    while (iterator.next()) {
        hash ^= (uint32_t) iterator.elementType;
        hash *= 1099511628211ull;
        if (iterator.elementType != Path::Iterator::closePath) {
            addPoint({ iterator.x1, iterator.y1 });
        }
        if (iterator.elementType == Path::Iterator::quadraticTo
                || iterator.elementType == Path::Iterator::cubicTo) {
            addPoint({ iterator.x2, iterator.y2 });
        }
        if (iterator.elementType == Path::Iterator::cubicTo) {
            addPoint({ iterator.x3, iterator.y3 });
        }
    }
    return hash;
}

}

bool NodeCanvasCableLayerCache::Entry::matches(
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        Rectangle<float> bounds,
        float currentZoom,
        float scale) const {
    return geometryFingerprint == cableGeometryFingerprint(edge)
            && destinationPortLike == edge.destinationPortLike
            && modulationBundle == edge.modulationBundle
            && destinationBundleIncludesYellow == edge.destinationBundleIncludesYellow
            && sourceEndpointVisible == edge.sourceEndpointVisible
            && destinationEndpointVisible == edge.destinationEndpointVisible
            && cableStyleEqual(styleSnapshot, style)
            && image.getWidth() == jmax(1, roundToInt(bounds.getWidth() * scale))
            && image.getHeight() == jmax(1, roundToInt(bounds.getHeight() * scale))
            && zoom == currentZoom
            && physicalScale == scale
            && image.isValid();
}

void NodeCanvasCableLayerCache::beginFrame(
        Rectangle<int> visibleBounds,
        float physicalScale) {
    if (paintGeneration != 0) {
        entries.erase(
                std::remove_if(
                        entries.begin(),
                        entries.end(),
                        [&](const Entry& entry) {
                            return entry.paintGeneration != paintGeneration;
                        }),
                entries.end());
    }
    ++paintGeneration;
    frameEntryIndices.clear();
    frameVisibleBounds = visibleBounds;
    framePhysicalScale = physicalScale;
    frameStats = {};
}

NodeCanvasCableLayerCacheAccess NodeCanvasCableLayerCache::access(
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        Rectangle<float> logicalBounds,
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
    entry->logicalBounds = logicalBounds;
    entry->paintGeneration = paintGeneration;
    frameEntryIndices.push_back(static_cast<size_t>(entry - entries.data()));
    return { &entry->image, logicalBounds, hit };
}

void NodeCanvasCableLayerCache::replaceEntry(
        Entry& entry,
        const NodeSceneEdge& sceneEdge,
        const NodeCableStyle& style,
        Rectangle<float> logicalBounds,
        float zoom,
        float physicalScale) {
    const int imageWidth = jmax(1, roundToInt(logicalBounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(logicalBounds.getHeight() * physicalScale));
    entry.edgeIndex = sceneEdge.edgeIndex;
    entry.geometryFingerprint = cableGeometryFingerprint(sceneEdge);
    entry.styleSnapshot = style;
    entry.logicalBounds = logicalBounds;
    entry.zoom = zoom;
    entry.physicalScale = physicalScale;
    entry.destinationPortLike = sceneEdge.destinationPortLike;
    entry.modulationBundle = sceneEdge.modulationBundle;
    entry.destinationBundleIncludesYellow = sceneEdge.destinationBundleIncludesYellow;
    entry.sourceEndpointVisible = sceneEdge.sourceEndpointVisible;
    entry.destinationEndpointVisible = sceneEdge.destinationEndpointVisible;
    ++entry.imageRevision;
    entry.image = Image(Image::ARGB, imageWidth, imageHeight, true);
}

void NodeCanvasCableLayerCache::rebuildComposite(Rectangle<int> bounds) {
    compositeBounds = bounds;
    compositePhysicalScale = framePhysicalScale;
    compositeEdgeIndices.clear();
    compositeEntryBounds.clear();
    compositeEntryRevisions.clear();
    compositeEdgeIndices.reserve(frameEntryIndices.size());
    compositeEntryBounds.reserve(frameEntryIndices.size());
    compositeEntryRevisions.reserve(frameEntryIndices.size());
    for (const size_t entryIndex : frameEntryIndices) {
        const Entry& entry = entries[entryIndex];
        compositeEdgeIndices.push_back(entry.edgeIndex);
        compositeEntryBounds.push_back(entry.logicalBounds);
        compositeEntryRevisions.push_back(entry.imageRevision);
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
    const float imageToLogicalX = entry.logicalBounds.getWidth()
            / (float) entry.image.getWidth();
    const float imageToLogicalY = entry.logicalBounds.getHeight()
            / (float) entry.image.getHeight();
    graphics.drawImageTransformed(
            entry.image,
            AffineTransform(
                    imageToLogicalX,
                    0.f,
                    entry.logicalBounds.getX(),
                    0.f,
                    imageToLogicalY,
                    entry.logicalBounds.getY()),
            false);
}

Rectangle<int> NodeCanvasCableLayerCache::frameCompositeBounds() const {
    Rectangle<float> bounds;
    for (const size_t entryIndex : frameEntryIndices) {
        const Entry& entry = entries[entryIndex];
        bounds = bounds.isEmpty()
                ? entry.logicalBounds
                : bounds.getUnion(entry.logicalBounds);
    }
    return bounds.getSmallestIntegerContainer().getIntersection(frameVisibleBounds);
}

bool NodeCanvasCableLayerCache::compositeMatches(Rectangle<int> bounds) const {
    if (frameStats.misses != 0
            || !compositeInitialized
            || compositeBounds != bounds
            || compositePhysicalScale != framePhysicalScale
            || compositeEdgeIndices.size() != frameEntryIndices.size()
            || compositeEntryBounds.size() != frameEntryIndices.size()
            || compositeEntryRevisions.size() != frameEntryIndices.size()) {
        return false;
    }
    for (size_t index = 0; index < frameEntryIndices.size(); ++index) {
        const Entry& entry = entries[frameEntryIndices[index]];
        if (entry.edgeIndex != compositeEdgeIndices[index]
                || entry.logicalBounds != compositeEntryBounds[index]
                || entry.imageRevision != compositeEntryRevisions[index]) {
            return false;
        }
    }
    return true;
}

bool NodeCanvasCableLayerCache::pendingLayoutMatches(Rectangle<int> bounds) const {
    if (!pendingLayoutInitialized
            || pendingBounds != bounds
            || pendingEdgeIndices.size() != frameEntryIndices.size()
            || pendingEntryBounds.size() != frameEntryIndices.size()
            || pendingEntryRevisions.size() != frameEntryIndices.size()) {
        return false;
    }
    for (size_t index = 0; index < frameEntryIndices.size(); ++index) {
        const Entry& entry = entries[frameEntryIndices[index]];
        if (entry.edgeIndex != pendingEdgeIndices[index]
                || entry.logicalBounds != pendingEntryBounds[index]
                || entry.imageRevision != pendingEntryRevisions[index]) {
            return false;
        }
    }
    return true;
}

void NodeCanvasCableLayerCache::rememberPendingLayout(Rectangle<int> bounds) {
    pendingBounds = bounds;
    pendingEdgeIndices.clear();
    pendingEntryBounds.clear();
    pendingEntryRevisions.clear();
    pendingEdgeIndices.reserve(frameEntryIndices.size());
    pendingEntryBounds.reserve(frameEntryIndices.size());
    pendingEntryRevisions.reserve(frameEntryIndices.size());
    for (const size_t entryIndex : frameEntryIndices) {
        const Entry& entry = entries[entryIndex];
        pendingEdgeIndices.push_back(entry.edgeIndex);
        pendingEntryBounds.push_back(entry.logicalBounds);
        pendingEntryRevisions.push_back(entry.imageRevision);
    }
    pendingLayoutInitialized = true;
}

NodeCanvasCableLayerCacheFrame NodeCanvasCableLayerCache::endFrame() {
    const Rectangle<int> bounds = frameCompositeBounds();
    const bool compositeHit = compositeMatches(bounds);
    bool drawEntries = false;
    if (compositeHit) {
        pendingLayoutInitialized = false;
    } else if (!compositeInitialized || pendingLayoutMatches(bounds)) {
        rebuildComposite(bounds);
        pendingLayoutInitialized = false;
    } else {
        rememberPendingLayout(bounds);
        drawEntries = true;
    }
    return { frameStats, &compositeImage, bounds, compositeHit, drawEntries };
}

void NodeCanvasCableLayerCache::drawComposite(
        Graphics& graphics,
        const NodeCanvasCableLayerCacheFrame& frame) const {
    if (frame.drawEntries) {
        for (const size_t entryIndex : frameEntryIndices) {
            drawEntry(graphics, entries[entryIndex]);
        }
        return;
    }
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
