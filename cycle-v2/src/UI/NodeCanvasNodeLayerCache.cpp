#include <cmath>

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
            && first.rationale.isNotEmpty() == second.rationale.isNotEmpty()
            && first.bounds.getWidth() == second.bounds.getWidth()
            && first.bounds.getHeight() == second.bounds.getHeight()
            && nodeParametersEqual(first.parameters, second.parameters)
            && portsEqual(first.inputs, second.inputs)
            && portsEqual(first.outputs, second.outputs)
            && first.reverseInputPortOrder == second.reverseInputPortOrder
            && first.model == second.model
            && first.editorState.equals(second.editorState);
}

bool runtimePreviewsEqual(const NodePreviewResult& first, const NodePreviewResult& second) {
    if (first.contentRevision != 0 && second.contentRevision != 0) {
        return first.contentRevision == second.contentRevision;
    }
    return nodePreviewResultsHaveEqualContent(first, second);
}

}

void NodeCanvasNodeLayerCache::clear() {
    entries.clear();
    frameStats = {};
}

bool NodeCanvasNodeLayerCache::Entry::matches(
        const Node& node,
        Rectangle<float> bounds,
        uint64_t resourceFingerprint,
        uint64_t contextFingerprint,
        const NodePreviewResult* runtimePreview,
        bool currentlySelected,
        float scale,
        bool moving) const {
    const bool nodeMatches = moving
            ? nodeSnapshot.model == node.model
                    && nodeSnapshot.kind == node.kind
                    && nodeSnapshot.bounds.getWidth() == node.bounds.getWidth()
                    && nodeSnapshot.bounds.getHeight() == node.bounds.getHeight()
            : nodesEqualForPresentation(nodeSnapshot, node);
    return nodeMatches
            && logicalSize == Point<float>(bounds.getWidth(), bounds.getHeight())
            && previewResourceFingerprint == resourceFingerprint
            && renderContextFingerprint == contextFingerprint
            && hasRuntimePreview == (runtimePreview != nullptr)
            && (runtimePreview == nullptr
                    || (moving && runtimePreview->contentRevision == 0
                            ? false
                            : runtimePreviewsEqual(runtimePreviewSnapshot, *runtimePreview)))
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
        Rectangle<float> logicalBounds,
        uint64_t previewResourceFingerprint,
        uint64_t renderContextFingerprint,
        const NodePreviewResult* runtimePreview,
        bool selected,
        float physicalScale) {
    auto match = entries.find(node.id);
    Entry* entry = match != entries.end() ? &match->second : nullptr;
    const bool hit = entry != nullptr
            && entry->matches(
                    node,
                    logicalBounds,
                    previewResourceFingerprint,
                    renderContextFingerprint,
                    runtimePreview,
                    selected,
                    physicalScale,
                    false);
    if (!hit) {
        if (entry == nullptr) {
            entry = &entries.try_emplace(node.id).first->second;
        }
        replaceEntry(
                *entry,
                node,
                logicalBounds,
                previewResourceFingerprint,
                renderContextFingerprint,
                runtimePreview,
                selected,
                physicalScale);
        ++frameStats.misses;
    } else {
        ++frameStats.hits;
    }
    entry->paintGeneration = paintGeneration;
    return { &entry->image, logicalBounds, hit };
}

NodeCanvasNodeLayerCacheAccess NodeCanvasNodeLayerCache::accessDuringMove(
        const Node& node,
        Rectangle<float> logicalBounds,
        const NodePreviewResult* runtimePreview,
        bool selected,
        float physicalScale) {
    // A node-move gesture only changes position; ordinary access checks the full key on release.
    auto match = entries.find(node.id);
    if (match == entries.end()) {
        return {};
    }
    Entry& entry = match->second;
    if (!entry.matches(
                node,
                logicalBounds,
                entry.previewResourceFingerprint,
                entry.renderContextFingerprint,
                runtimePreview,
                selected,
                physicalScale,
                true)) {
        return {};
    }
    entry.paintGeneration = paintGeneration;
    ++frameStats.hits;
    return { &entry.image, logicalBounds, true };
}

void NodeCanvasNodeLayerCache::replaceEntry(
        Entry& entry,
        const Node& node,
        Rectangle<float> logicalBounds,
        uint64_t previewResourceFingerprint,
        uint64_t renderContextFingerprint,
        const NodePreviewResult* runtimePreview,
        bool selected,
        float physicalScale) {
    const int imageWidth = jmax(1, roundToInt(logicalBounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(logicalBounds.getHeight() * physicalScale));
    entry.nodeSnapshot = node;
    entry.hasRuntimePreview = runtimePreview != nullptr;
    entry.runtimePreviewSnapshot = runtimePreview != nullptr
            ? *runtimePreview
            : NodePreviewResult {};
    entry.logicalSize = { logicalBounds.getWidth(), logicalBounds.getHeight() };
    entry.previewResourceFingerprint = previewResourceFingerprint;
    entry.renderContextFingerprint = renderContextFingerprint;
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
    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    const float imageToLogical = 1.f / jmax(1.f, physicalScale);
    const float alignedX = std::round(access.logicalBounds.getX() * physicalScale)
            * imageToLogical;
    const float alignedY = std::round(access.logicalBounds.getY() * physicalScale)
            * imageToLogical;
    Graphics::ScopedSaveState saved(graphics);
    graphics.setOpacity(1.f);
    graphics.drawImageTransformed(
            *access.image,
            AffineTransform(
                    imageToLogical,
                    0.f,
                    alignedX,
                    0.f,
                    imageToLogical,
                    alignedY),
            false);
}

NodeCanvasNodeLayerCacheStats NodeCanvasNodeLayerCache::endFrame() {
    for (auto entry = entries.begin(); entry != entries.end();) {
        if (entry->second.paintGeneration != paintGeneration) {
            entry = entries.erase(entry);
        } else {
            ++entry;
        }
    }
    return frameStats;
}

}
