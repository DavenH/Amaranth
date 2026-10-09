#include "UI/GraphEndpointNodeRenderer.h"

#include "Graph/NodeGraph.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/NodeIconRenderer.h"
#include "UI/NodePortGeometry.h"

namespace CycleV2 {

namespace {

Rectangle<float> captionBounds(
        NodeKind kind,
        Rectangle<float> nodeBounds,
        Rectangle<float> viewportBounds,
        float zoom) {
    const float width = 76.f * zoom;
    const float gap = 8.f * zoom;
    const float edgeInset = 36.f * zoom;
    const float x = kind == NodeKind::VoiceOutput
            ? nodeBounds.getRight() + gap
            : nodeBounds.getX() - gap - width;
    Rectangle<float> caption(x, nodeBounds.getCentreY() - 24.f * zoom,
            width, 48.f * zoom);
    if (kind == NodeKind::VoiceOutput
            && caption.getRight() > viewportBounds.getRight() - edgeInset) {
        caption.setPosition(nodeBounds.getCentreX() - width * 0.5f,
                nodeBounds.getY() - caption.getHeight() - gap);
    } else if (kind == NodeKind::GlobalInput
            && caption.getX() < viewportBounds.getX() + edgeInset) {
        caption.setPosition(nodeBounds.getCentreX() - width * 0.5f,
                nodeBounds.getBottom() + gap);
    }
    return caption;
}

}

bool GraphEndpointNodeRenderer::isConnector(NodeKind kind) {
    return kind == NodeKind::VoiceOutput || kind == NodeKind::GlobalInput;
}

Rectangle<float> GraphEndpointNodeRenderer::visualBounds(
        NodeKind kind,
        Rectangle<float> nodeBounds,
        Rectangle<float> viewportBounds,
        float zoom) {
    return isConnector(kind)
            ? nodeBounds.getUnion(captionBounds(kind, nodeBounds, viewportBounds, zoom))
            : nodeBounds;
}

void GraphEndpointNodeRenderer::paintConnector(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> nodeBounds,
        Rectangle<float> viewportBounds,
        float zoom,
        bool selected) {
    const float scale = zoom / NodePortGeometry::referenceZoom;
    graphics.setColour(CanvasChromePalette::surface);
    graphics.fillEllipse(nodeBounds);
    graphics.setColour(CanvasChromePalette::strongBorder);
    graphics.drawEllipse(nodeBounds, jmax(1.f, 1.5f * scale));

    if (selected) {
        graphics.setColour(CanvasChromePalette::selectionOutline.withAlpha(0.16f));
        graphics.drawEllipse(nodeBounds.expanded(4.f * zoom), 3.f * scale);
        graphics.setColour(CanvasChromePalette::selectionOutline.withAlpha(0.92f));
        graphics.drawEllipse(nodeBounds.expanded(2.f * zoom),
                CanvasChromeMetrics::activeBorderWidth);
    }

    const Rectangle<float> icon = Rectangle<float>(52.f * zoom, 52.f * zoom)
            .withCentre(nodeBounds.getCentre());
    NodeIconRenderer::paint(graphics, kind, icon, 1.f);

    Rectangle<float> caption = captionBounds(kind, nodeBounds, viewportBounds, zoom);
    const Rectangle<float> firstLine = caption.removeFromTop(24.f * zoom);
    const bool voiceOutput = kind == NodeKind::VoiceOutput;
    graphics.setFont(FontOptions(21.f * zoom));
    graphics.setColour(CanvasChromePalette::text);
    graphics.drawText(voiceOutput ? "VOICE" : "GLOBAL",
            firstLine, Justification::centred);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.drawText(voiceOutput ? "OUT" : "IN",
            caption, Justification::centred);
}

}
