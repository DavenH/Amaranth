#include <cmath>
#include <cstring>

#include "UI/NodeCanvasPresentation.h"
#include "Runtime/FingerprintBuilder.h"
#include "UI/NodeCableRenderer.h"
#include "UI/NodeCanvasGlRenderer.h"
#include "UI/EnvelopePurposeIconRenderer.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodePortGeometry.h"
#include "UI/NodeViewModule.h"
#include "UI/VoiceContextCompactEditor.h"
#include "Graph/GraphRenderSemanticResolver.h"
#include "Graph/GraphValidator.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

namespace {

uint64_t unisonContextFingerprint(const UnisonPreviewContext& context) {
    uint64_t durationBits {};
    std::memcpy(&durationBits, &context.voiceDurationSeconds, sizeof(durationBits));
    FingerprintBuilder fingerprint;
    fingerprint
            .add((uint64_t) context.midiNote)
            .add(durationBits)
            .add(context.pitchEnvelopeUnitValues.size());
    for (const float value : context.pitchEnvelopeUnitValues) {
        uint32_t valueBits {};
        std::memcpy(&valueBits, &value, sizeof(valueBits));
        fingerprint.add(valueBits);
    }
    return fingerprint.value();
}

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor { 0x2f5b6370 };
const Colour kCanvasGridMinor { 0x182f363f };
const Colour kNodeBackground { 0xff171d24 };
const Colour kNodeHeader { 0xff202833 };
const Colour kNodeBorder { 0xff3d4a58 };
const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
float portScale(float zoom) {
    return zoom / NodePortGeometry::referenceZoom;
}

void paintConfigurationSocket(
        Graphics& graphics,
        Point<float> centre,
        float scale,
        Colour colour,
        bool input) {
    const Rectangle<float> socket = Rectangle<float>(
            NodePortGeometry::socketDiameter * scale,
            NodePortGeometry::socketDiameter * scale)
            .withCentre(centre);
    graphics.setColour(kCanvasBackground.withAlpha(0.96f));
    graphics.fillRoundedRectangle(socket.expanded(2.f * scale), 2.5f * scale);
    graphics.setColour(colour.withAlpha(0.26f));
    graphics.fillRoundedRectangle(socket.expanded(1.f * scale), 2.f * scale);
    graphics.setColour(input ? colour : kCanvasBackground.withAlpha(0.96f));
    graphics.fillRoundedRectangle(socket, 1.8f * scale);
    graphics.setColour(colour);
    graphics.drawRoundedRectangle(socket, 1.8f * scale, 1.4f * scale);
}

void paintRoundSocket(
        Graphics& graphics,
        Rectangle<float> bounds,
        float scale,
        Colour colour,
        bool input) {
    graphics.setColour(colour.withAlpha(0.22f));
    graphics.fillEllipse(bounds.expanded(1.4f * scale));
    if (input) {
        graphics.setColour(colour);
        graphics.fillEllipse(bounds);
        return;
    }

    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(bounds);
    graphics.setColour(colour);
    graphics.drawEllipse(bounds, 1.2f * scale);
}

void paintAttachmentSocket(
        Graphics& graphics,
        Point<float> centre,
        float scale,
        const Port& port,
        Colour fallbackColour) {
    if (port.attachmentType == AttachmentType::ModulationTriple) {
        NodeCableRenderer::paintModulationSocket(
                graphics,
                centre,
                NodePortGeometry::socketDiameter * scale,
                !port.input);
        return;
    }

    if (port.attachmentType == AttachmentType::Unison) {
        const float radius = NodePortGeometry::socketDiameter * scale * 0.5f;
        paintRoundSocket(
                graphics,
                Rectangle<float>(radius * 2.f, radius * 2.f).withCentre(centre),
                scale,
                EffectPlotPalette::accent,
                port.input);
        return;
    }

    paintConfigurationSocket(graphics, centre, scale, fallbackColour, port.input);
}

String modulationParameterId(const String& prefix, const String& name) {
    if (prefix.isEmpty()) {
        return name;
    }
    return prefix + name.substring(0, 1).toUpperCase() + name.substring(1);
}

String modulationSourceLabel(const Node& node, const String& prefix = {}) {
    const String source = parameterValueForNode(
            node,
            modulationParameterId(prefix, "source"),
            "modWheel");
    if (source == "voiceTime") {
        return "Voice Time";
    }
    if (source == "velocity") {
        return "Velocity";
    }
    if (source == "inverseVelocity") {
        return "1-Velocity";
    }
    if (source == "keyScale") {
        return "Key Scale";
    }
    if (source == "channelPressure") {
        return "Pressure";
    }
    if (source == "midiCC") {
        return "CC " + parameterValueForNode(
                node,
                modulationParameterId(prefix, "controller"),
                "1");
    }
    if (source == "constant") {
        return parameterValueForNode(
                node,
                modulationParameterId(prefix, "constant"),
                "0.5");
    }
    return "Mod Wheel";
}

void paintModulationShell(
        Graphics& graphics,
        Rectangle<float> bounds,
        float corner,
        bool selected) {
    graphics.setColour(kNodeBackground);
    graphics.fillRoundedRectangle(bounds, corner);
    graphics.setColour(kNodeBorder);
    graphics.drawRoundedRectangle(bounds, corner, 1.2f);
    if (selected) {
        graphics.setColour(Colours::white.withAlpha(0.86f));
        graphics.drawRoundedRectangle(bounds.expanded(2.f), corner + 2.f, 2.f);
    }
}

void paintSingleModulationNode(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame,
        const Node& node,
        Rectangle<float> bounds,
        float zoom,
        float scale) {
    paintModulationShell(graphics, bounds, 8.f * scale, node.id == frame.selectedNodeId);
    const Rectangle<float> badge = bounds.removeFromLeft(52.f * zoom);
    graphics.setColour(kNodeHeader);
    graphics.fillRoundedRectangle(badge, 8.f * scale);
    graphics.fillRect(badge.withTrimmedLeft(badge.getWidth() - 8.f * scale));
    graphics.setColour(kMutedText);
    graphics.setFont(FontOptions(12.f * zoom));
    graphics.drawText("MOD", badge, Justification::centred);

    graphics.setColour(kText);
    graphics.setFont(FontOptions(20.f * zoom));
    graphics.drawText(
            modulationSourceLabel(node),
            bounds.reduced(14.f * zoom, 2.f * zoom),
            Justification::centredLeft);

    const Point<float> centre {
            frame.viewport.toScreen(node.bounds).getRight(),
            frame.viewport.toScreen(node.bounds).getCentreY()
    };
    const float diameter = 8.4f * scale;
    graphics.setColour(kCanvasBackground.withAlpha(0.92f));
    graphics.fillEllipse(Rectangle<float>(diameter, diameter).withCentre(centre));
    graphics.setColour(colourForDomain(PortDomain::ControlSignal));
    graphics.drawEllipse(Rectangle<float>(diameter, diameter).withCentre(centre), 1.2f * scale);
}

void paintTripleModulationNode(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame,
        const Node& node,
        Rectangle<float> bounds,
        float zoom,
        float scale) {
    paintModulationShell(graphics, bounds, 8.f * scale, node.id == frame.selectedNodeId);
    const String prefixes[] { "yellow", "red", "blue" };
    const MorphDimension dimensions[] {
            MorphDimension::Yellow,
            MorphDimension::Red,
            MorphDimension::Blue
    };
    const float rowHeight = bounds.getHeight() / 3.f;
    for (int row = 0; row < 3; ++row) {
        Rectangle<float> rowBounds(
                bounds.getX(),
                bounds.getY() + rowHeight * (float) row,
                bounds.getWidth(),
                rowHeight);
        graphics.setColour(colourForMorphDimension(dimensions[row]).withAlpha(0.88f));
        graphics.fillRoundedRectangle(rowBounds.removeFromLeft(6.f * zoom), 3.f * scale);
        graphics.setFont(FontOptions(14.f * zoom));
        graphics.drawText(prefixes[row].substring(0, 1).toUpperCase(),
                rowBounds.removeFromLeft(30.f * zoom), Justification::centred);
        graphics.setColour(kText);
        graphics.setFont(FontOptions(19.f * zoom));
        graphics.drawText(
                modulationSourceLabel(node, prefixes[row]),
                rowBounds.reduced(6.f * zoom, 2.f * zoom),
                Justification::centredLeft);
        if (row < 2) {
            graphics.setColour(kNodeBorder.withAlpha(0.72f));
            graphics.drawHorizontalLine(
                    roundToInt(rowBounds.getBottom()),
                    bounds.getX() + 10.f * zoom,
                    bounds.getRight() - 12.f * zoom);
        }
    }

    NodeCableRenderer::paintModulationSocket(
            graphics,
            frame.viewport.toScreen(
                    ModulationCableBundle::worldCentre(node, false)),
            ModulationCableBundle::socketDiameter * scale,
            true);
}

void paintEnvelopePurposeIcon(
        Graphics& graphics,
        const Node& node,
        Rectangle<float> header,
        float zoom) {
    const EnvelopePurpose purpose = envelopePurposeFor(node);
    const Rectangle<float> icon = header.reduced(10.f * zoom, 6.f * zoom)
            .removeFromRight(31.f * zoom);
    EnvelopePurposeIconRenderer::paint(graphics, purpose, icon);
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
    return edge.isAttachment() ? edge.domain : GraphValidator().resolvedDomainForEdge(graph, edge);
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
        NodePreviewRenderer& previewRendererToUse,
        NodeCanvasPresentationPerformanceObserver* performanceObserverToUse) :
        scene(sceneToUse)
    ,   previewRenderer(previewRendererToUse)
    ,   signalProbeRail(previewRendererToUse, performanceObserverToUse)
    ,   signalProbeDetailView(previewRendererToUse)
    ,   performanceObserver(performanceObserverToUse) {
}

void NodeCanvasPresentation::paint(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::Backdrop);
        paintGrid(graphics, frame);
        GuideRelationshipPresentation::paintTether(graphics, frame);
    }

    {
        Graphics::ScopedSaveState contentClip(graphics);
        graphics.reduceClipRegion(frame.canvasBounds.toNearestInt());
        if (!frame.canvasOcclusion.isEmpty()) {
            graphics.excludeClipRegion(frame.canvasOcclusion.toNearestInt());
        }

        paintContent(graphics, frame);
    }

    const WorkspaceDockLayout dock = WorkspaceDock::layout(
            frame.workspaceBounds,
            {
                    frame.probeRailState.expanded,
                    frame.guideShelfState.minimized,
                    frame.probeRailState.minimized,
                    frame.probeRailState.expandedHeight,
                    frame.dockSplitRatio
            });
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::GuideShelf);
        if (frame.probeRailState.expanded) {
            guideCurveShelf.paint(
                    graphics,
                    frame.graph,
                    frame.workspaceBounds,
                    frame.probeRailState,
                    frame.dockSplitRatio,
                    frame.guideShelfState,
                    frame.dockFocus);
        }
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::SpyRail);
        signalProbeRail.paintRail(
                graphics,
                frame.graph,
                frame.previewResult,
                frame.probeRailState.expanded
                        ? GuideCurveShelf::spyWorkspace(
                                frame.workspaceBounds,
                                frame.dockSplitRatio,
                                frame.guideShelfState.minimized,
                                frame.probeRailState.minimized)
                        : frame.workspaceBounds,
                frame.probeRailState,
                frame.dockFocus);
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::DockAndDetail);
        WorkspaceDock::paintChrome(
                graphics,
                dock,
                "Curve Guides",
                "Spies",
                frame.probeRailState.expanded,
                frame.dockFocus.target == WorkspaceDockFocusTarget::Collapse);
        GuideRelationshipPresentation::paintTetherTerminal(graphics, frame);
        signalProbeDetailView.paint(
                graphics,
                frame.canvasBounds,
                frame.probeDetailState);
    }
}

void NodeCanvasPresentation::paintContent(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::SnapGuides);
        paintSnapGuides(graphics, frame);
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::Cables);
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::CableBodies);
            paintEdges(graphics, frame);
        }
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::CableAnnotations);
            signalProbeRail.paintCableAnnotations(
                    graphics,
                    frame.graph,
                    scene.snapshot(),
                    GuideCurveShelf::spyWorkspace(
                            frame.workspaceBounds,
                            frame.dockSplitRatio,
                            frame.guideShelfState.minimized,
                            frame.probeRailState.minimized),
                    frame.probeRailState);
        }
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::PendingConnection);
        paintPendingConnection(graphics, frame);
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::Nodes);
        paintCachedNodes(graphics, frame);
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::RelationshipHighlights);
        GuideRelationshipPresentation::paintHighlights(graphics, frame);
    }
    {
        ScopedNodeCanvasPresentationStage measurement(
                performanceObserver,
                NodeCanvasPresentationStage::Utilities);
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::MiniMap);
            paintMiniMap(graphics, frame);
        }
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::Legend);
            paintLegend(graphics, frame);
        }
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::Palette);
            paintPalette(graphics, frame);
        }
        {
            ScopedNodeCanvasPresentationStage childMeasurement(
                    performanceObserver,
                    NodeCanvasPresentationStage::Status);
            paintStatus(graphics, frame);
        }
    }
}

bool NodeCanvasPresentation::renderOpenGL(
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
    return guideCurveShelf.renderOpenGL(
            frame.graph,
            frame.workspaceBounds,
            frame.canvasBounds,
            frame.probeRailState,
            frame.dockSplitRatio,
            frame.guideShelfState,
            scaleFactor);
}

bool NodeCanvasPresentation::guideShelfNeedsOpenGLPreviewRender() const {
    return guideCurveShelf.needsOpenGLPreviewRender();
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
    const uint64_t startedAt = performanceObserver != nullptr
            ? performanceObserver->presentationTimestamp()
            : 0;
    const float zoom = frame.viewport.getZoom();
    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    const auto& edges = frame.graph.getEdges();
    const auto& snapshot = scene.build(
            frame.graph,
            frame.viewport,
            frame.presentationRevision,
            frame.documentRevision);

    cableLayerCache.beginFrame();
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
        const NodeCableStyle style {
                colour,
                invalid,
                sceneEdge.edgeIndex == frame.selectedEdgeIndex,
                sceneEdge.edgeIndex == frame.spliceTargetEdgeIndex,
                sceneEdge.modulationBundle
        };
        paintCachedEdge(graphics, sceneEdge, style, zoom, physicalScale);
    }
    const NodeCanvasCableLayerCacheStats stats = cableLayerCache.endFrame();
    if (performanceObserver != nullptr) {
        performanceObserver->cableLayerCacheCompleted(
                stats.hits,
                stats.misses,
                performanceObserver->presentationTimestamp() - startedAt);
    }
}

void NodeCanvasPresentation::paintCachedEdge(
        Graphics& graphics,
        const NodeSceneEdge& sceneEdge,
        const NodeCableStyle& style,
        float zoom,
        float physicalScale) {
    const Rectangle<int> logicalBounds = NodeCableRenderer::visibleBounds(sceneEdge, zoom)
            .getSmallestIntegerContainer();
    const NodeCanvasCableLayerCacheAccess cache = cableLayerCache.access(
            sceneEdge,
            style,
            logicalBounds,
            zoom,
            physicalScale);
    if (!cache.hit) {
        Graphics imageGraphics(*cache.image);
        imageGraphics.addTransform(AffineTransform(
                physicalScale,
                0.f,
                -logicalBounds.getX() * physicalScale,
                0.f,
                physicalScale,
                -logicalBounds.getY() * physicalScale));
        NodeCableRenderer::paint(imageGraphics, sceneEdge, style, zoom);
    }
    cableLayerCache.draw(graphics, cache);
}

void NodeCanvasPresentation::paintPendingConnection(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (!frame.pendingConnection.has_value()) {
        return;
    }

    const auto& pending = *frame.pendingConnection;
    const Node* node = findNode(frame.graph, pending.source.nodeId);
    if (node != nullptr && ModulationCableBundle::isAddress(pending.source)) {
        const Point<float> start = frame.viewport.toScreen(
                ModulationCableBundle::worldCentre(*node, pending.source.input));
        const Point<float> source = pending.source.input ? pending.pointer : start;
        const Point<float> destination = pending.source.input ? start : pending.pointer;
        NodeCableRenderer::paintPending(graphics, {
                NodeCanvasScene::cablePath(
                        source,
                        destination,
                        PortSide::Right,
                        PortSide::Left,
                        frame.viewport.getZoom()),
                source,
                destination,
                colourForDomain(PortDomain::ControlSignal),
                true,
                node->kind != NodeKind::Envelope
        }, frame.viewport.getZoom());
        return;
    }
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

void NodeCanvasPresentation::paintCachedNodes(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const uint64_t startedAt = performanceObserver != nullptr
            ? performanceObserver->presentationTimestamp()
            : 0;
    nodeLayerCache.beginFrame();
    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    const Rectangle<float> visibleArea = frame.canvasBounds.expanded(120.f);
    for (const Node& node : frame.graph.getNodes()) {
        const Rectangle<float> nodeBounds = frame.viewport.toScreen(node.bounds);
        if (!nodeBounds.intersects(visibleArea)) {
            continue;
        }
        paintCachedNode(graphics, frame, node, physicalScale);
    }
    const NodeCanvasNodeLayerCacheStats stats = nodeLayerCache.endFrame();
    if (performanceObserver != nullptr) {
        performanceObserver->nodeLayerCacheCompleted(
                stats.hits,
                stats.misses,
                performanceObserver->presentationTimestamp() - startedAt);
    }
}

void NodeCanvasPresentation::paintCachedNode(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame,
        const Node& node,
        float physicalScale) {
    const Rectangle<int> logicalBounds = frame.viewport.toScreen(node.bounds)
            .expanded(32.f * portScale(frame.viewport.getZoom()))
            .getSmallestIntegerContainer();
    const bool usesGlobalContext = node.kind == NodeKind::Unison
            || node.kind == NodeKind::VoiceContext;
    const NodeCanvasNodeLayerCacheAccess cache = nodeLayerCache.access(
            node,
            logicalBounds,
            frame.presentationRevision,
            frame.viewport.getRevision(),
            previewRenderer.nodePresentationFingerprint(node.id),
            usesGlobalContext ? unisonContextFingerprint(frame.unisonPreviewContext) : 0,
            node.id == frame.selectedNodeId,
            physicalScale);
    if (!cache.hit) {
        Graphics imageGraphics(*cache.image);
        imageGraphics.addTransform(AffineTransform(
                physicalScale,
                0.f,
                -logicalBounds.getX() * physicalScale,
                0.f,
                physicalScale,
                -logicalBounds.getY() * physicalScale));
        paintNode(imageGraphics, frame, node);
    }
    nodeLayerCache.draw(graphics, cache);
}

UnisonPreviewContext NodeCanvasPresentation::unisonPreviewContextFor(
        const GraphExecutionPlan& plan,
        const String& unisonNodeId,
        UnisonPreviewContext fallback) {
    std::vector<const Edge*> attachments;
    for (const auto& edge : plan.configurationAttachments) {
        if (edge.sourceNodeId == unisonNodeId
                && edge.attachmentType == AttachmentType::Unison) {
            attachments.push_back(&edge);
        }
    }
    if (attachments.size() != 1) {
        return fallback;
    }
    const Edge* attachment = attachments.front();
    const auto context = std::find_if(
            plan.voiceContexts.begin(),
            plan.voiceContexts.end(),
            [&](const CompiledVoiceContext& candidate) {
                return candidate.nodeId == attachment->destNodeId;
            });
    if (context != plan.voiceContexts.end()) {
        fallback.pitchEnvelopeUnitValues = context->pitchEnvelopeUnitValues;
    }
    return fallback;
}

void NodeCanvasPresentation::paintNode(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame,
        const Node& node) {
    const float zoom = frame.viewport.getZoom();
    const float scale = portScale(zoom);
    const float corner = 8.f * scale;
    const Rectangle<float> nodeBounds = frame.viewport.toScreen(node.bounds);
    if (node.kind == NodeKind::ModulationSource) {
        paintSingleModulationNode(graphics, frame, node, nodeBounds, zoom, scale);
        return;
    }
    if (node.kind == NodeKind::ModulationTriple) {
        paintTripleModulationNode(graphics, frame, node, nodeBounds, zoom, scale);
        return;
    }

    if (node.kind == NodeKind::SpectralLayer) {
        previewRenderer.paint(graphics, {
                node,
                nullptr,
                nodeBounds.reduced(12.f * zoom),
                profileFor(frame, node),
                zoom,
                true,
                frame.unisonPreviewContext
        });
        graphics.setColour(kNodeBorder.withAlpha(0.7f));
        graphics.drawEllipse(nodeBounds.reduced(2.f * zoom), 1.2f * scale);
        if (node.id == frame.selectedNodeId) {
            graphics.setColour(Colours::white.withAlpha(0.86f));
            graphics.drawEllipse(nodeBounds.expanded(2.f * zoom), 2.f * scale);
        }
    } else {
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

        graphics.setFont(FontOptions(18.f * zoom));
        graphics.setColour(kText);
        graphics.drawText(labelForNodeKind(node.kind), header.reduced(13.f * zoom, 4.f * zoom),
                          Justification::centredLeft);
        if (node.kind == NodeKind::Envelope) {
            paintEnvelopePurposeIcon(graphics, node, header, zoom);
        }
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
            VoiceContextCompactEditor::paintNodeSummary(
                    graphics,
                    nodeBounds,
                    zoom,
                    node,
                    frame.unisonPreviewContext.voiceDurationSeconds);
        } else {
            previewRenderer.paint(graphics, {
                    node,
                    previewFor(frame.previewResult, node.id),
                    preview,
                    profileFor(frame, node),
                    zoom,
                    true,
                    node.kind == NodeKind::Unison
                            ? unisonPreviewContextFor(
                                    frame.compileResult.plan,
                                    node.id,
                                    frame.unisonPreviewContext)
                            : frame.unisonPreviewContext
            });
        }
    }

    const auto paintPort = [&](const Port& port) {
        const NodePortPresentation location = portPresentation(frame.viewport, node, port);
        const Colour colour = displayColour(node, port);
        if (port.connectionKind == ConnectionKind::ConfigurationAttachment) {
            paintAttachmentSocket(
                    graphics,
                    location.centre,
                    scale,
                    port,
                    colour);
            return;
        }
        paintRoundSocket(graphics, location.bounds, scale, colour, port.input);
    };

    for (const auto& port : node.inputs) {
        if (!ModulationCableBundle::hidesIndividualPort(node, port)) {
            paintPort(port);
        }
    }

    for (const auto& port : node.outputs) {
        paintPort(port);
    }

    if (ModulationCableBundle::supportsDestination(node)) {
        NodeCableRenderer::paintModulationSocket(
                graphics,
                frame.viewport.toScreen(
                        ModulationCableBundle::worldCentre(node, true)),
                ModulationCableBundle::socketDiameter * scale,
                false,
                ModulationCableBundle::destinationIncludesYellow(node));
    }
}

NodePortPresentation NodeCanvasPresentation::portPresentation(
        const NodeCanvasViewport& viewport,
        const Node& node,
        const Port& port) {
    const Point<float> centre = viewport.toScreen(NodeCanvasScene::portWorldCentre(node, port));
    const float radius = NodePortGeometry::socketDiameter
            * portScale(viewport.getZoom())
            * 0.5f;
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
