#include <algorithm>

#include "UI/SignalProbePreviewTileCache.h"

namespace CycleV2 {

namespace {

bool previewsEqual(
        const GraphPreviewResult::SignalProbePreview& first,
        const GraphPreviewResult::SignalProbePreview& second) {
    return first.probeId == second.probeId
            && first.values == second.values
            && first.gridColumns == second.gridColumns
            && first.gridRows == second.gridRows
            && first.domain == second.domain
            && first.channelLayout == second.channelLayout
            && first.sourceRole == second.sourceRole
            && first.frequencySampling == second.frequencySampling
            && first.frequencyMidiNote == second.frequencyMidiNote
            && first.connected == second.connected;
}

bool semanticsEqual(NodeRenderSemantic first, NodeRenderSemantic second) {
    return first.domain == second.domain
            && first.scalePolicy == second.scalePolicy
            && first.role == second.role;
}

}

void SignalProbePreviewTileCache::clear() {
    entries.clear();
    frameStats = {};
}

bool SignalProbePreviewTileCache::Entry::matches(
        const GraphPreviewResult::SignalProbePreview& preview,
        NodeRenderSemantic semantic,
        Rectangle<int> bounds,
        float scale) const {
    return previewsEqual(previewSnapshot, preview)
            && semanticsEqual(semanticSnapshot, semantic)
            && logicalBounds == bounds
            && physicalScale == scale
            && image.isValid();
}

void SignalProbePreviewTileCache::beginFrame() {
    ++paintGeneration;
    frameStats = {};
}

SignalProbePreviewTileCacheAccess SignalProbePreviewTileCache::access(
        const GraphPreviewResult::SignalProbePreview& preview,
        NodeRenderSemantic semantic,
        Rectangle<int> logicalBounds,
        float physicalScale) {
    auto match = std::find_if(
            entries.begin(),
            entries.end(),
            [&](const Entry& entry) { return entry.probeId == preview.probeId; });
    Entry* entry = match != entries.end() ? &*match : nullptr;
    const bool hit = entry != nullptr
            && entry->matches(preview, semantic, logicalBounds, physicalScale);
    if (!hit) {
        if (entry == nullptr) {
            entries.push_back({});
            entry = &entries.back();
        }
        replaceEntry(*entry, preview, semantic, logicalBounds, physicalScale);
        ++frameStats.misses;
    } else {
        ++frameStats.hits;
    }
    entry->paintGeneration = paintGeneration;
    return { &entry->image, logicalBounds, hit };
}

void SignalProbePreviewTileCache::replaceEntry(
        Entry& entry,
        const GraphPreviewResult::SignalProbePreview& preview,
        NodeRenderSemantic semantic,
        Rectangle<int> logicalBounds,
        float physicalScale) {
    const int imageWidth = jmax(1, roundToInt(logicalBounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(logicalBounds.getHeight() * physicalScale));
    entry.probeId = preview.probeId;
    entry.previewSnapshot = preview;
    entry.semanticSnapshot = semantic;
    entry.logicalBounds = logicalBounds;
    entry.physicalScale = physicalScale;
    entry.image = Image(Image::ARGB, imageWidth, imageHeight, true);
}

void SignalProbePreviewTileCache::draw(
        Graphics& graphics,
        const SignalProbePreviewTileCacheAccess& access) const {
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

SignalProbePreviewTileCacheStats SignalProbePreviewTileCache::endFrame() {
    entries.erase(
            std::remove_if(
                    entries.begin(),
                    entries.end(),
                    [&](const Entry& entry) { return entry.paintGeneration != paintGeneration; }),
            entries.end());
    return frameStats;
}

}
