#include "NodeCableRenderer.h"

namespace CycleV2 {

namespace {

const Colour kCanvasBackground { 0xff101318 };
constexpr float kCableReferenceZoom = 0.58f;
constexpr float kCableStrokeScale = 0.70f;

void paintSpliceMarker(
        Graphics& graphics,
        const Path& cable,
        Colour colour,
        float scale) {
    const Point<float> midpoint = cable.getPointAlongPath(cable.getLength() * 0.5f);
    const float markerSize = 17.f * scale;
    const Rectangle<float> marker(markerSize, markerSize);
    const Rectangle<float> placed = marker.withCentre(midpoint);

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(placed);
    graphics.setColour(colour.withAlpha(0.98f));
    graphics.drawEllipse(placed, 2.2f * scale);
    graphics.drawLine(
            placed.getX() + markerSize * 0.30f,
            midpoint.y,
            placed.getRight() - markerSize * 0.30f,
            midpoint.y,
            1.9f * scale);
    graphics.drawLine(
            midpoint.x,
            placed.getY() + markerSize * 0.30f,
            midpoint.x,
            placed.getBottom() - markerSize * 0.30f,
            1.9f * scale);
}

void paintAttachmentCable(
        Graphics& graphics,
        const Path& cable,
        const NodeCableStyle& style,
        float scale) {
    Path dashedCable;
    const PathStrokeType dashStroke(
            2.f * scale,
            PathStrokeType::curved,
            PathStrokeType::rounded);
    const Array<float> dashes { 8.f * scale, 7.f * scale };
    dashStroke.createDashedStroke(
            dashedCable,
            cable,
            dashes.getRawDataPointer(),
            dashes.size());

    graphics.setColour(style.colour.withAlpha(
            style.spliceTarget ? 0.62f : (style.selected ? 0.46f : 0.32f)));
    graphics.strokePath(
            dashedCable,
            PathStrokeType(
                    (style.spliceTarget ? 13.f : (style.selected ? 10.f : 7.f)) * scale,
                    PathStrokeType::curved,
                    PathStrokeType::rounded));
    graphics.setColour(style.colour.withAlpha(0.92f));
    graphics.strokePath(
            dashedCable,
            PathStrokeType(
                    (style.spliceTarget ? 4.5f : (style.selected ? 3.f : 2.f)) * scale,
                    PathStrokeType::curved,
                    PathStrokeType::rounded));
}

void paintSignalCable(
        Graphics& graphics,
        const Path& cable,
        const NodeCableStyle& style,
        float scale) {
    graphics.setColour(style.colour.withAlpha(
            style.spliceTarget ? 0.42f : (style.selected ? 0.28f : 0.18f)));
    graphics.strokePath(
            cable,
            PathStrokeType(
                    (style.spliceTarget ? 15.f : (style.selected ? 12.f : 9.f)) * scale,
                    PathStrokeType::curved,
                    PathStrokeType::rounded));
    graphics.setColour(style.colour.withAlpha(0.92f));
    graphics.strokePath(
            cable,
            PathStrokeType(
                    (style.spliceTarget ? 5.2f : (style.selected ? 4.f : 3.f)) * scale,
                    PathStrokeType::curved,
                    PathStrokeType::rounded));

    if (!style.invalid) {
        return;
    }

    Path dashedCable;
    const PathStrokeType dashStroke(
            1.8f * scale,
            PathStrokeType::curved,
            PathStrokeType::rounded);
    const Array<float> dashes { 5.f * scale, 6.f * scale };
    dashStroke.createDashedStroke(
            dashedCable,
            cable,
            dashes.getRawDataPointer(),
            dashes.size());
    graphics.setColour(Colours::white.withAlpha(0.58f));
    graphics.strokePath(
            dashedCable,
            PathStrokeType(
                    1.4f * scale,
                    PathStrokeType::curved,
                    PathStrokeType::rounded));
}

void paintEndpoints(
        Graphics& graphics,
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        float scale) {
    const float endpointSize = (style.spliceTarget ? 15.f : (style.selected ? 14.f : 11.f)) * scale;
    const Rectangle<float> endpoint(endpointSize, endpointSize);
    const Rectangle<float> sourceMarker = endpoint.withCentre(edge.source);
    const Rectangle<float> destinationMarker = endpoint.withCentre(edge.destination);

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(sourceMarker);
    graphics.setColour(style.colour.withAlpha(0.96f));
    graphics.drawEllipse(
            sourceMarker,
            (style.spliceTarget ? 2.8f : (style.selected ? 2.4f : 1.8f)) * scale);

    if (edge.destinationPortLike) {
        graphics.fillEllipse(destinationMarker.reduced(
                (style.spliceTarget ? 1.2f : (style.selected ? 1.5f : 2.f)) * scale));
        return;
    }

    Path badge;
    const float radius = (style.selected ? 7.f : 5.8f) * scale;
    badge.startNewSubPath(edge.destination.x, edge.destination.y - radius);
    badge.lineTo(edge.destination.x + radius, edge.destination.y);
    badge.lineTo(edge.destination.x, edge.destination.y + radius);
    badge.lineTo(edge.destination.x - radius, edge.destination.y);
    badge.closeSubPath();

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillPath(badge);
    graphics.setColour(style.colour.withAlpha(style.selected ? 0.98f : 0.78f));
    graphics.strokePath(
            badge,
            PathStrokeType((style.selected ? 2.2f : 1.6f) * scale));
}

}

Rectangle<float> NodeCableRenderer::visibleBounds(const NodeSceneEdge& edge, float zoom) {
    return edge.cablePath.getBounds().expanded(18.f * scaleForZoom(zoom));
}

float NodeCableRenderer::scaleForZoom(float zoom) {
    return zoom / kCableReferenceZoom * kCableStrokeScale;
}

void NodeCableRenderer::paint(
        Graphics& graphics,
        const NodeSceneEdge& edge,
        const NodeCableStyle& style,
        float zoom) {
    const float scale = scaleForZoom(zoom);

    if (style.attachment) {
        paintAttachmentCable(graphics, edge.cablePath, style, scale);
    } else {
        paintSignalCable(graphics, edge.cablePath, style, scale);
    }

    if (style.spliceTarget) {
        paintSpliceMarker(graphics, edge.cablePath, style.colour, scale);
    }

    paintEndpoints(graphics, edge, style, scale);
}

void NodeCableRenderer::paintPending(
        Graphics& graphics,
        const PendingNodeConnection& connection,
        float zoom) {
    const float scale = scaleForZoom(zoom);

    graphics.setColour(connection.colour.withAlpha(0.18f));
    graphics.strokePath(
            connection.path,
            PathStrokeType(9.f * scale, PathStrokeType::curved, PathStrokeType::rounded));
    graphics.setColour(connection.colour.withAlpha(0.88f));
    graphics.strokePath(
            connection.path,
            PathStrokeType(3.f * scale, PathStrokeType::curved, PathStrokeType::rounded));

    const float markerSize = 11.f * scale;
    const Rectangle<float> marker(markerSize, markerSize);
    const Rectangle<float> sourceMarker = marker.withCentre(connection.source);
    const Rectangle<float> destinationMarker = marker.withCentre(connection.destination);

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(sourceMarker);
    graphics.setColour(connection.colour.withAlpha(0.96f));
    graphics.drawEllipse(sourceMarker, 1.8f * scale);
    graphics.fillEllipse(destinationMarker.reduced(2.f * scale));
}

}
