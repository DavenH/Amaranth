#include "NodeCanvasPresentation.h"

#include "NodeCableRenderer.h"
#include "NodeCanvasGlRenderer.h"
#include "NodeViewModule.h"
#include "VoiceContextCompactEditor.h"
#include "../Graph/GraphRenderSemanticResolver.h"
#include "../Graph/GraphValidator.h"

#include <cmath>

namespace CycleV2 {

namespace {

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor { 0x2f5b6370 };
const Colour kCanvasGridMinor { 0x182f363f };
const Colour kNodeBackground { 0xff171d24 };
const Colour kNodeHeader { 0xff202833 };
const Colour kNodeBorder { 0xff3d4a58 };
const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kCableReferenceZoom = 0.58f;

float portScale(float zoom) {
    return zoom / kCableReferenceZoom;
}

Colour displayColour(const Node& node, const Port& port) {
    if (port.input && (node.kind == NodeKind::Envelope || node.kind == NodeKind::TrilinearMesh)) {
        if (port.id == "yellow") {
            return colourForMorphDimension(MorphDimension::Yellow);
        }
        if (port.id == "red") {
            return colourForMorphDimension(MorphDimension::Red);
        }
        if (port.id == "blue") {
            return colourForMorphDimension(MorphDimension::Blue);
        }
    }

    const auto& capabilities = NodeViewModuleRegistry::instance().moduleFor(node.kind).capabilities();
    if (capabilities.operationLayoutControl) {
        return colourForDomain(PortDomain::ControlSignal);
    }

    return colourForDomain(port.domain);
}

const Port* findPort(const Node& node, const PortAddress& address) {
    const auto& ports = address.input ? node.inputs : node.outputs;
    for (const auto& port : ports) {
        if (port.id == address.portId) {
            return &port;
        }
    }

    return nullptr;
}

const Node* findNode(const NodeGraph& graph, const String& id) {
    for (const auto& node : graph.getNodes()) {
        if (node.id == id) {
            return &node;
        }
    }

    return nullptr;
}

PortDomain edgeDomain(const NodeGraph& graph, const Edge& edge) {
    return edge.attachment ? edge.domain : GraphValidator().resolvedDomainForEdge(graph, edge);
}

Rectangle<float> actionButton(Rectangle<float> nodeBounds, float zoom) {
    const float size = 27.f * zoom;
    return Rectangle<float>(size, size).withCentre({
            nodeBounds.getRight() - 21.f * zoom,
            nodeBounds.getY() + 21.f * zoom
    });
}

enum class OperationPortLayout {
    Side,
    Uptack,
    Vertical,
    Tee
};

OperationPortLayout operationLayout(const Node& node) {
    if (node.inputs.size() < 2) {
        return OperationPortLayout::Side;
    }

    const PortSide first = node.inputs[0].side;
    const PortSide second = node.inputs[1].side;
    if (first == PortSide::Left && second == PortSide::Bottom) {
        return OperationPortLayout::Tee;
    }
    if (first == PortSide::Top && second == PortSide::Bottom) {
        return OperationPortLayout::Vertical;
    }
    if (first == PortSide::Left && second == PortSide::Top) {
        return OperationPortLayout::Uptack;
    }

    return OperationPortLayout::Side;
}

OperationPortLayout nextLayout(OperationPortLayout layout) {
    switch (layout) {
        case OperationPortLayout::Side:     return OperationPortLayout::Uptack;
        case OperationPortLayout::Uptack:   return OperationPortLayout::Vertical;
        case OperationPortLayout::Vertical: return OperationPortLayout::Tee;
        case OperationPortLayout::Tee:      return OperationPortLayout::Side;
    }

    return OperationPortLayout::Side;
}

PortSide nextOutputSide(const Node& node) {
    const PortSide side = node.outputs.empty() ? PortSide::Right : node.outputs.front().side;
    switch (side) {
        case PortSide::Right:  return PortSide::Bottom;
        case PortSide::Bottom: return PortSide::Top;
        case PortSide::Top:    return PortSide::Right;
        case PortSide::Left:   return PortSide::Right;
    }

    return PortSide::Right;
}

void paintActionButton(Graphics& graphics, Rectangle<float> button, float scale) {
    graphics.setColour(Colour(0xff0f141a).withAlpha(0.78f));
    graphics.fillEllipse(button);
    graphics.setColour(kMutedText.withAlpha(0.82f));
    graphics.drawEllipse(button, scale);
}

void paintOperationAction(
        Graphics& graphics,
        Rectangle<float> button,
        float scale,
        OperationPortLayout layout) {
    paintActionButton(graphics, button, scale);

    const Rectangle<float> icon = button.reduced(button.getWidth() * 0.24f);
    const float stroke = jmax(1.f, button.getWidth() * 0.085f);
    const float dot = jmax(2.f, button.getWidth() * 0.14f);
    Point<float> first;
    Point<float> second;
    const Point<float> output(icon.getRight(), icon.getCentreY());

    switch (layout) {
        case OperationPortLayout::Vertical:
            first = { icon.getCentreX(), icon.getY() };
            second = { icon.getCentreX(), icon.getBottom() };
            break;
        case OperationPortLayout::Tee:
            first = { icon.getX(), icon.getCentreY() };
            second = { icon.getCentreX(), icon.getBottom() };
            break;
        case OperationPortLayout::Uptack:
            first = { icon.getX(), icon.getCentreY() };
            second = { icon.getCentreX(), icon.getY() };
            break;
        case OperationPortLayout::Side:
            first = { icon.getX(), icon.getY() + icon.getHeight() * 0.30f };
            second = { icon.getX(), icon.getBottom() - icon.getHeight() * 0.30f };
            break;
    }

    Path path;
    path.startNewSubPath(first);
    path.lineTo(icon.getCentre());
    path.lineTo(second);
    path.startNewSubPath(icon.getCentre());
    path.lineTo(output);
    graphics.setColour(kMutedText.withAlpha(0.78f));
    graphics.strokePath(path, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
    graphics.setColour(kMutedText);
    graphics.drawEllipse(Rectangle<float>(dot, dot).withCentre(first), stroke);
    graphics.drawEllipse(Rectangle<float>(dot, dot).withCentre(second), stroke);
    graphics.fillEllipse(Rectangle<float>(dot, dot).withCentre(output));
}

void paintOutputAction(
        Graphics& graphics,
        Rectangle<float> button,
        float scale,
        PortSide side) {
    paintActionButton(graphics, button, scale);

    const Rectangle<float> icon = button.reduced(button.getWidth() * 0.24f);
    const float stroke = jmax(1.f, button.getWidth() * 0.085f);
    const float dot = jmax(2.f, button.getWidth() * 0.14f);
    const Point<float> centre = icon.getCentre();
    Point<float> output;

    if (side == PortSide::Top) {
        output = { centre.x, icon.getY() };
    } else if (side == PortSide::Bottom) {
        output = { centre.x, icon.getBottom() };
    } else {
        output = { icon.getRight(), centre.y };
    }

    graphics.setColour(kMutedText.withAlpha(0.78f));
    graphics.drawEllipse(Rectangle<float>(dot, dot).withCentre(centre), stroke);
    graphics.drawLine(Line<float>(centre, output), stroke);
    graphics.setColour(kMutedText);
    graphics.fillEllipse(Rectangle<float>(dot, dot).withCentre(output));
}

}

NodeCanvasPresentation::NodeCanvasPresentation(
        NodeCanvasScene& sceneToUse,
        NodePreviewRenderer& previewRendererToUse) :
        scene(sceneToUse)
    ,   previewRenderer(previewRendererToUse)
    ,   signalProbeRail(previewRendererToUse) {
}

void NodeCanvasPresentation::paint(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    paintGrid(graphics, frame);

    {
        Graphics::ScopedSaveState contentClip(graphics);
        graphics.reduceClipRegion(frame.canvasBounds.toNearestInt());
        if (!frame.canvasOcclusion.isEmpty()) {
            graphics.excludeClipRegion(frame.canvasOcclusion.toNearestInt());
        }

        paintContent(graphics, frame);
    }

    signalProbeRail.paintRail(
            graphics,
            frame.graph,
            frame.previewResult,
            frame.workspaceBounds,
            frame.probeRailState);
}

void NodeCanvasPresentation::paintContent(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    paintSnapGuides(graphics, frame);
    paintEdges(graphics, frame);
    signalProbeRail.paintCableAnnotations(
            graphics,
            frame.graph,
            scene.snapshot(),
            frame.workspaceBounds,
            frame.probeRailState);
    paintPendingConnection(graphics, frame);
    paintNodes(graphics, frame);
    paintMiniMap(graphics, frame);
    paintLegend(graphics, frame);
    paintPalette(graphics, frame);
    paintHoverConsole(graphics, frame);
}

void NodeCanvasPresentation::renderOpenGL(
        NodeCanvasRenderer& renderer,
        const NodeCanvasPresentationFrame& frame,
        float scaleFactor) {
    renderer.renderBackground(
            frame.canvasBounds,
            frame.workspaceBounds.getHeight(),
            scaleFactor,
            frame.viewport.getZoom(),
            frame.viewport.getPan());
    renderOpenGLEffectPreviews(frame, scaleFactor);
}

void NodeCanvasPresentation::paintGrid(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (frame.openGLUnderlay) {
        return;
    }

    graphics.fillAll(kCanvasBackground);

    const float minorStep = 32.f * frame.viewport.getZoom();
    const float majorStep = minorStep * 4.f;
    const Point<float> pan = frame.viewport.getPan();
    const auto paintLines = [&](float step, Colour colour) {
        graphics.setColour(colour);
        float startX = std::fmod(pan.x, step);
        float startY = std::fmod(pan.y, step);
        startX -= startX > 0.f ? step : 0.f;
        startY -= startY > 0.f ? step : 0.f;

        for (float x = startX; x < frame.canvasBounds.getRight(); x += step) {
            graphics.drawVerticalLine(roundToInt(x), 0.f, frame.canvasBounds.getHeight());
        }

        for (float y = startY; y < frame.canvasBounds.getBottom(); y += step) {
            graphics.drawHorizontalLine(roundToInt(y), 0.f, frame.canvasBounds.getWidth());
        }
    };

    paintLines(minorStep, kCanvasGridMinor);
    paintLines(majorStep, kCanvasGridMajor);
}

void NodeCanvasPresentation::paintEdges(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const float zoom = frame.viewport.getZoom();
    const auto& edges = frame.graph.getEdges();
    const auto& snapshot = scene.build(
            frame.graph,
            frame.viewport,
            frame.presentationRevision,
            frame.documentRevision);

    for (const auto& sceneEdge : snapshot.edges) {
        if (sceneEdge.edgeIndex < 0 || sceneEdge.edgeIndex >= (int) edges.size()) {
            continue;
        }

        if (!NodeCableRenderer::visibleBounds(sceneEdge, zoom)
                    .intersects(frame.canvasBounds.expanded(160.f))) {
            continue;
        }

        const auto& edge = edges[(size_t) sceneEdge.edgeIndex];
        const bool invalid = GraphValidator().edgeHasValidationIssue(frame.graph, edge);
        const Colour colour = invalid
                ? Colour(0xffff5a5f)
                : colourForDomain(edgeDomain(frame.graph, edge));
        NodeCableRenderer::paint(graphics, sceneEdge, {
                colour,
                edge.attachment,
                invalid,
                sceneEdge.edgeIndex == frame.selectedEdgeIndex,
                sceneEdge.edgeIndex == frame.spliceTargetEdgeIndex
        }, zoom);
    }
}

void NodeCanvasPresentation::paintPendingConnection(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (!frame.pendingConnection.has_value()) {
        return;
    }

    const auto& pending = *frame.pendingConnection;
    const Node* node = findNode(frame.graph, pending.source.nodeId);
    const Port* port = node == nullptr ? nullptr : findPort(*node, pending.source);
    if (node == nullptr || port == nullptr) {
        return;
    }

    const Point<float> start = portPresentation(frame.viewport, *node, *port).centre;
    const Point<float> source = pending.source.input ? pending.pointer : start;
    const Point<float> destination = pending.source.input ? start : pending.pointer;
    const PortSide sourceSide = pending.source.input ? PortSide::Right : port->side;
    const PortSide destinationSide = pending.source.input ? port->side : PortSide::Left;
    NodeCableRenderer::paintPending(graphics, {
            NodeCanvasScene::cablePath(
                    source,
                    destination,
                    sourceSide,
                    destinationSide,
                    frame.viewport.getZoom()),
            source,
            destination,
            colourForDomain(port->domain)
    }, frame.viewport.getZoom());
}

void NodeCanvasPresentation::paintSnapGuides(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const auto& guides = frame.snapGuides;
    if (guides.hasX) {
        const float x = frame.viewport.toScreen(Point<float>(guides.worldX, 0.f)).x;
        graphics.setColour(colourForDomain(PortDomain::TimeSignal).withAlpha(0.18f));
        graphics.drawLine(x, 0.f, x, frame.canvasBounds.getHeight(), 0.75f);
    }

    if (guides.hasY) {
        const float y = frame.viewport.toScreen(Point<float>(0.f, guides.worldY)).y;
        graphics.setColour(colourForDomain(PortDomain::PitchSignal).withAlpha(0.16f));
        graphics.drawLine(0.f, y, frame.canvasBounds.getWidth(), y, 0.75f);
    }
}

void NodeCanvasPresentation::paintNodes(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const Rectangle<float> visibleArea = frame.canvasBounds.expanded(120.f);
    for (const auto& node : frame.graph.getNodes()) {
        if (frame.viewport.toScreen(node.bounds).intersects(visibleArea)) {
            paintNode(graphics, frame, node);
        }
    }
}

void NodeCanvasPresentation::paintNode(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame,
        const Node& node) {
    const float zoom = frame.viewport.getZoom();
    const float scale = portScale(zoom);
    const float corner = 8.f * scale;
    const Rectangle<float> nodeBounds = frame.viewport.toScreen(node.bounds);
    Rectangle<float> body = nodeBounds;
    const Rectangle<float> header = body.removeFromTop(42.f * zoom);

    graphics.setColour(kNodeBackground);
    graphics.fillRoundedRectangle(nodeBounds, corner);
    graphics.setColour(kNodeHeader);
    graphics.fillRoundedRectangle(header, corner);
    graphics.fillRect(header.withTrimmedTop(header.getHeight() - corner));
    graphics.setColour(kNodeBorder);
    graphics.drawRoundedRectangle(nodeBounds, corner, 1.2f);

    if (node.id == frame.selectedNodeId) {
        graphics.setColour(Colours::white.withAlpha(0.86f));
        graphics.drawRoundedRectangle(nodeBounds.expanded(2.f), corner + 2.f, 2.f);
    }

    graphics.setFont(FontOptions(15.f * zoom, Font::bold));
    graphics.setColour(kText);
    graphics.drawText(node.title, header.reduced(13.f * zoom, 4.f * zoom), Justification::centredLeft);

    const auto& capabilities = NodeViewModuleRegistry::instance().moduleFor(node.kind).capabilities();
    if (capabilities.operationLayoutControl) {
        paintOperationAction(
                graphics,
                actionButton(nodeBounds, zoom),
                scale,
                nextLayout(operationLayout(node)));
    } else if (capabilities.outputSideControl) {
        paintOutputAction(
                graphics,
                actionButton(nodeBounds, zoom),
                scale,
                nextOutputSide(node));
    }

    const Rectangle<float> preview = previewRenderer.boundsFor(node, nodeBounds, zoom);
    if (node.kind == NodeKind::VoiceContext) {
        VoiceContextCompactEditor::paintNodeSelector(graphics, nodeBounds, zoom, node);
    } else {
        previewRenderer.paint(graphics, {
                node,
                previewFor(frame.previewResult, node.id),
                preview,
                profileFor(frame, node),
                zoom,
                true
        });
    }

    const auto paintPort = [&](const Port& port) {
        const NodePortPresentation location = portPresentation(frame.viewport, node, port);
        const Colour colour = displayColour(node, port);
        graphics.setColour(colour.withAlpha(0.22f));
        graphics.fillEllipse(location.bounds.expanded(1.4f * scale));

        if (port.input) {
            graphics.setColour(colour);
            graphics.fillEllipse(location.bounds);
        } else {
            graphics.setColour(kCanvasBackground.withAlpha(0.92f));
            graphics.fillEllipse(location.bounds);
            graphics.setColour(colour);
            graphics.drawEllipse(location.bounds, 1.2f * scale);
        }
    };

    for (const auto& port : node.inputs) {
        paintPort(port);
    }

    for (const auto& port : node.outputs) {
        paintPort(port);
    }
}

NodePortPresentation NodeCanvasPresentation::portPresentation(
        const NodeCanvasViewport& viewport,
        const Node& node,
        const Port& port) {
    const Point<float> centre = viewport.toScreen(NodeCanvasScene::portWorldCentre(node, port));
    const float radius = 4.2f * portScale(viewport.getZoom());
    return {
            Rectangle<float>(centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f),
            centre
    };
}

const NodePreviewResult* NodeCanvasPresentation::previewFor(
        const GraphPreviewResult& previews,
        const String& nodeId) const {
    for (const auto& preview : previews.nodes) {
        if (preview.nodeId == nodeId) {
            return &preview;
        }
    }

    return nullptr;
}

TrimeshRenderProfile NodeCanvasPresentation::profileFor(
        const NodeCanvasPresentationFrame& frame,
        const Node& node) const {
    NodeRenderSemantic semantic = GraphRenderSemanticResolver().semanticForNodeOutput(
            frame.graph,
            node.id,
            "out");
    if (semantic.domain == PortDomain::ControlSignal && !node.outputs.empty()) {
        semantic.domain = node.outputs.front().domain;
    }

    return TrimeshRenderProfile::fromSemantic(semantic);
}

void NodeCanvasPresentation::renderOpenGLEffectPreviews(
        const NodeCanvasPresentationFrame& frame,
        float scaleFactor) {
    const Rectangle<float> visibleArea = frame.canvasBounds.expanded(120.f);
    for (const auto& node : frame.graph.getNodes()) {
        const Rectangle<float> nodeBounds = frame.viewport.toScreen(node.bounds);
        if (!nodeBounds.intersects(visibleArea)
                || (!frame.canvasOcclusion.isEmpty()
                    && frame.canvasOcclusion.intersects(nodeBounds))) {
            continue;
        }

        previewRenderer.renderOpenGL(
                node,
                previewRenderer.boundsFor(node, nodeBounds, frame.viewport.getZoom()),
                scaleFactor);
    }
}

}
