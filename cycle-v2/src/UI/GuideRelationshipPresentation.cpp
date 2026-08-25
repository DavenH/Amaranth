#include "GuideRelationshipPresentation.h"

#include "GuideCurveShelf.h"
#include "NodeCanvasPresentation.h"

namespace CycleV2 {

namespace {

const Colour kCanvasBackground { 0xff101318 };

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

bool isVisibleTarget(
        const NodeCanvasPresentationFrame& frame,
        const Node& node) {
    const Rectangle<float> bounds = frame.viewport.toScreen(node.bounds);
    const bool hiddenByEditor = !frame.canvasOcclusion.isEmpty()
            && bounds.intersects(frame.canvasOcclusion);
    return bounds.intersects(frame.canvasBounds) && !hiddenByEditor;
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

    for (const auto& nodeId : frame.graph.guideTargetNodeIds(guideId)) {
        const Node* target = frame.graph.findNode(nodeId);
        if (target == nullptr) {
            continue;
        }
        const Rectangle<float> bounds = frame.viewport.toScreen(target->bounds);
        if (bounds.intersects(frame.canvasBounds)) {
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
    if (guide == nullptr) {
        return;
    }

    const Rectangle<float> tile = GuideCurveShelf::tileBoundsFor(
            frame.workspaceBounds,
            frame.probeRailState,
            frame.dockSplitRatio,
            frame.guideShelfState,
            guide->shelfOrder);
    const Point<float> start { tile.getCentreX(), tile.getY() };
    Path tethers;
    int visibleTargetCount = 0;
    for (const auto& nodeId : frame.graph.guideTargetNodeIds(guideId)) {
        const Node* target = frame.graph.findNode(nodeId);
        if (target == nullptr || !isVisibleTarget(frame, *target)) {
            continue;
        }

        const Rectangle<float> destination = frame.viewport.toScreen(target->bounds);
        const Point<float> end { destination.getCentreX(), destination.getBottom() };
        const float controlDistance = jmax(48.f, (start.y - end.y) * 0.35f);
        tethers.startNewSubPath(start);
        tethers.cubicTo(
                start.x, start.y - controlDistance,
                end.x, end.y + controlDistance,
                end.x, end.y);
        ++visibleTargetCount;
    }
    if (visibleTargetCount == 0) {
        return;
    }

    Graphics::ScopedSaveState overlayClip(graphics);
    if (!frame.canvasOcclusion.isEmpty()) {
        graphics.excludeClipRegion(frame.canvasOcclusion.toNearestInt());
    }
    graphics.setColour(GuideCurveShelf::colourForGuide(*guide).withAlpha(0.48f));
    graphics.strokePath(tethers, PathStrokeType(1.6f));
}

}
