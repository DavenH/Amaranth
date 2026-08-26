#include "UI/GuideRelationshipPresentation.h"

#include "UI/GuideCurveShelf.h"
#include "UI/NodeCanvasPresentation.h"
#include "UI/WorkspaceDock.h"

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
    graphics.setFont(FontOptions(10.f));
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

Point<float> tetherStart(
        const NodeCanvasPresentationFrame& frame,
        const GuideCurveResource& guide) {
    const Rectangle<float> tile = GuideCurveShelf::tileBoundsFor(
            frame.workspaceBounds,
            frame.probeRailState,
            frame.dockSplitRatio,
            frame.guideShelfState,
            guide.shelfOrder);
    const WorkspaceDockLayout dock = WorkspaceDock::layout(
            frame.workspaceBounds,
            {
                    frame.probeRailState.expanded,
                    frame.guideShelfState.minimized,
                    frame.probeRailState.minimized,
                    frame.probeRailState.expandedHeight,
                    frame.dockSplitRatio
            });
    return { tile.getCentreX(), dock.dock.getY() };
}

bool hasVisibleTarget(
        const NodeCanvasPresentationFrame& frame,
        const String& guideId) {
    for (const auto& nodeId : frame.graph.guideTargetNodeIds(guideId)) {
        const Node* target = frame.graph.findNode(nodeId);
        if (target != nullptr && isVisibleTarget(frame, *target)) {
            return true;
        }
    }
    return false;
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

    const Point<float> start = tetherStart(frame, *guide);
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

void GuideRelationshipPresentation::paintTetherTerminal(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (!frame.probeRailState.expanded || frame.guideShelfState.minimized) {
        return;
    }

    const String guideId = tetherGuideId(frame.guideShelfState);
    const GuideCurveResource* guide = frame.graph.findGuideCurve(guideId);
    if (guide == nullptr || !hasVisibleTarget(frame, guideId)) {
        return;
    }

    const Point<float> start = tetherStart(frame, *guide);
    graphics.setColour(Colour(0xff101318).withAlpha(0.96f));
    graphics.fillRoundedRectangle(Rectangle<float>(16.f, 8.f).withCentre(start), 4.f);
    graphics.setColour(GuideCurveShelf::colourForGuide(*guide).withAlpha(0.92f));
    graphics.fillRoundedRectangle(Rectangle<float>(11.f, 4.f).withCentre(start), 2.f);
}

}
