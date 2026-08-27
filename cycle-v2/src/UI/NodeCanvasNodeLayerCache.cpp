#include <algorithm>

#include "UI/NodeCanvasNodeLayerCache.h"

namespace CycleV2 {

namespace {

bool nodeParametersEqual(
        const std::vector<NodeParameter>& first,
        const std::vector<NodeParameter>& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (size_t index = 0; index < first.size(); ++index) {
        if (first[index].id != second[index].id
                || first[index].label != second[index].label
                || first[index].value != second[index].value) {
            return false;
        }
    }
    return true;
}

bool portsEqual(const std::vector<Port>& first, const std::vector<Port>& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (size_t index = 0; index < first.size(); ++index) {
        const Port& left = first[index];
        const Port& right = second[index];
        if (left.id != right.id
                || left.label != right.label
                || left.domain != right.domain
                || left.channelLayout != right.channelLayout
                || left.purpose != right.purpose
                || left.input != right.input
                || left.side != right.side
                || left.connectionKind != right.connectionKind
                || left.attachmentType != right.attachmentType
                || left.defaultModulationSlot != right.defaultModulationSlot) {
            return false;
        }
    }
    return true;
}

bool nodesEqualForPresentation(const Node& first, const Node& second) {
    return first.id == second.id
            && first.kind == second.kind
            && first.subtitle == second.subtitle
            && first.bounds == second.bounds
            && nodeParametersEqual(first.parameters, second.parameters)
            && portsEqual(first.inputs, second.inputs)
            && portsEqual(first.outputs, second.outputs)
            && first.model == second.model
            && first.editorState.equals(second.editorState);
}

}

bool NodeCanvasNodeLayerCache::Entry::matches(
        const Node& node,
        Rectangle<int> bounds,
        uint64_t currentPresentationRevision,
        uint64_t currentViewportRevision,
        uint64_t resourceFingerprint,
        uint64_t contextFingerprint,
        bool currentlySelected,
        float scale) const {
    return nodesEqualForPresentation(nodeSnapshot, node)
            && logicalBounds == bounds
            && presentationRevision == currentPresentationRevision
            && viewportRevision == currentViewportRevision
            && previewResourceFingerprint == resourceFingerprint
            && unisonContextFingerprint == contextFingerprint
            && physicalScale == scale
            && selected == currentlySelected
            && image.isValid();
}

void NodeCanvasNodeLayerCache::beginFrame() {
    ++paintGeneration;
    frameStats = {};
}

NodeCanvasNodeLayerCacheAccess NodeCanvasNodeLayerCache::access(
        const Node& node,
        Rectangle<int> logicalBounds,
        uint64_t presentationRevision,
        uint64_t viewportRevision,
        uint64_t previewResourceFingerprint,
        uint64_t unisonContextFingerprint,
        bool selected,
        float physicalScale) {
    auto match = std::find_if(
            entries.begin(),
            entries.end(),
            [&](const Entry& entry) { return entry.nodeId == node.id; });
    Entry* entry = match != entries.end() ? &*match : nullptr;
    const bool hit = entry != nullptr
            && entry->matches(
                    node,
                    logicalBounds,
                    presentationRevision,
                    viewportRevision,
                    previewResourceFingerprint,
                    unisonContextFingerprint,
                    selected,
                    physicalScale);
    if (!hit) {
        if (entry == nullptr) {
            entries.push_back({});
            entry = &entries.back();
        }
        replaceEntry(
                *entry,
                node,
                logicalBounds,
                presentationRevision,
                viewportRevision,
                previewResourceFingerprint,
                unisonContextFingerprint,
                selected,
                physicalScale);
        ++frameStats.misses;
    } else {
        ++frameStats.hits;
    }
    entry->paintGeneration = paintGeneration;
    return { &entry->image, logicalBounds, hit };
}

void NodeCanvasNodeLayerCache::replaceEntry(
        Entry& entry,
        const Node& node,
        Rectangle<int> logicalBounds,
        uint64_t presentationRevision,
        uint64_t viewportRevision,
        uint64_t previewResourceFingerprint,
        uint64_t unisonContextFingerprint,
        bool selected,
        float physicalScale) {
    const int imageWidth = jmax(1, roundToInt(logicalBounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(logicalBounds.getHeight() * physicalScale));
    entry.nodeId = node.id;
    entry.nodeSnapshot = node;
    entry.logicalBounds = logicalBounds;
    entry.presentationRevision = presentationRevision;
    entry.viewportRevision = viewportRevision;
    entry.previewResourceFingerprint = previewResourceFingerprint;
    entry.unisonContextFingerprint = unisonContextFingerprint;
    entry.physicalScale = physicalScale;
    entry.selected = selected;
    entry.image = Image(Image::ARGB, imageWidth, imageHeight, true);
}

void NodeCanvasNodeLayerCache::draw(
        Graphics& graphics,
        const NodeCanvasNodeLayerCacheAccess& access) const {
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

NodeCanvasNodeLayerCacheStats NodeCanvasNodeLayerCache::endFrame() {
    entries.erase(
            std::remove_if(
                    entries.begin(),
                    entries.end(),
                    [&](const Entry& entry) { return entry.paintGeneration != paintGeneration; }),
            entries.end());
    return frameStats;
}

}
