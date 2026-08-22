#include "GuideRelationshipPresentation.h"

#include "GuideCurveShelf.h"
#include "NodeCanvasPresentation.h"

#include <unordered_set>

namespace CycleV2 {

namespace {

const Colour kCanvasBackground { 0xff101318 };

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

void paintHighlight(
        Graphics& graphics,
        Rectangle<float> bounds,
        const GuideCurveResource& guide) {
    const Colour colour = GuideCurveShelf::colourForGuide(guide);
    graphics.setColour(colour.withAlpha(0.9f));
    graphics.drawRoundedRectangle(bounds.expanded(4.f), 10.f, 2.f);

    const Rectangle<float> badge(
            bounds.getRight() - 37.f,
            bounds.getY() + 5.f,
            32.f,
            17.f);
    graphics.setColour(kCanvasBackground.withAlpha(0.94f));
    graphics.fillRoundedRectangle(badge, 5.f);
    graphics.setColour(colour);
    graphics.drawRoundedRectangle(badge, 5.f, 1.f);
    graphics.setFont(FontOptions(10.f, Font::bold));
    graphics.drawText(guide.shortLabel, badge, Justification::centred);
}

const Node* firstVisibleTarget(
        const NodeCanvasPresentationFrame& frame,
        const String& guideId) {
    for (const auto& nodeId : frame.graph.guideTargetNodeIds(guideId)) {
        const Node* node = frame.graph.findNode(nodeId);
        if (node != nullptr
                && frame.viewport.toScreen(node->bounds).intersects(frame.canvasBounds)) {
            return node;
        }
    }
    return nullptr;
}

}

String GuideRelationshipPresentation::highlightGuideId(
        const GuideCurveShelfState& state) {
    return state.hoveredGuideId.isNotEmpty()
            ? state.hoveredGuideId
            : state.selectedGuideId;
}

String GuideRelationshipPresentation::tetherGuideId(
        const GuideCurveShelfState& state) {
    return state.hoveredGuideId;
}

void GuideRelationshipPresentation::paintHighlights(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const String guideId = highlightGuideId(frame.guideShelfState);
    const GuideCurveResource* guide = frame.graph.findGuideCurve(guideId);
    if (guide == nullptr) {
        return;
    }

    std::unordered_set<String, StringHash> targetNodeIds;
    for (const auto& nodeId : frame.graph.guideTargetNodeIds(guideId)) {
        targetNodeIds.insert(nodeId);
    }
    for (const auto& node : frame.graph.getNodes()) {
        const Rectangle<float> bounds = frame.viewport.toScreen(node.bounds);
        if (targetNodeIds.find(node.id) != targetNodeIds.end()
                && bounds.intersects(frame.canvasBounds)) {
            paintHighlight(graphics, bounds, *guide);
        }
    }
}

void GuideRelationshipPresentation::paintTether(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (!frame.probeRailState.expanded || frame.guideShelfState.minimized) {
        return;
    }

    const String guideId = tetherGuideId(frame.guideShelfState);
    const GuideCurveResource* guide = frame.graph.findGuideCurve(guideId);
    const Node* target = firstVisibleTarget(frame, guideId);
    if (guide == nullptr || target == nullptr) {
        return;
    }

    const Rectangle<float> tile = GuideCurveShelf::tileBoundsFor(
            frame.workspaceBounds,
            frame.probeRailState,
            frame.dockSplitRatio,
            frame.guideShelfState,
            guide->shelfOrder);
    const Rectangle<float> destination = frame.viewport.toScreen(target->bounds);
    const Point<float> start { tile.getCentreX(), tile.getY() };
    const Point<float> end { destination.getCentreX(), destination.getBottom() };
    const float controlDistance = jmax(48.f, (start.y - end.y) * 0.35f);
    Path tether;
    tether.startNewSubPath(start);
    tether.cubicTo(
            start.x, start.y - controlDistance,
            end.x, end.y + controlDistance,
            end.x, end.y);
    graphics.setColour(GuideCurveShelf::colourForGuide(*guide).withAlpha(0.48f));
    graphics.strokePath(tether, PathStrokeType(1.6f));
}

}
