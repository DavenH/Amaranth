#include "NodeCanvas.h"
#include "NodeViewModule.h"
#include "TransformCompactEditor.h"

#include "../Runtime/GraphAudioExecutor.h"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

namespace CycleV2 {

namespace NodeCanvasInvalidation {

constexpr uint32_t CanvasRepaint = 1u << 0;

}

namespace {

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor  { 0x2f5b6370 };
const Colour kCanvasGridMinor  { 0x182f363f };
constexpr bool kUseGlCanvasUnderlay = true;

bool hasHostedEditor(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().hostedEditor;
}

GraphDocument createStartupDocument() {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    return GraphDocument::openOrDefault(defaultGraph, NodeGraph::createDemoGraph());
  #else
    return GraphDocument(NodeGraph::createDemoGraph());
  #endif
}

}

NodeCanvas::NodeCanvas() :
        document(createStartupDocument())
    ,   commands(document)
    ,   graph(document.graph())
    ,   compileResult(presentation.compileResult())
    ,   runtimeTrace(presentation.runtimeTrace())
    ,   previewResult(presentation.previewResult())
    ,   queries(graph, compileResult, runtimeTrace, previewResult)
    ,   editorCommands(*this, document, commands, *this, *this)
    ,   authoring(document, commands, presentation, editorCommands)
    ,   selectedNodeId(authoring.interactionSession().selectedNodeId)
    ,   expandedNodeId(authoring.interactionSession().expandedNodeId)
    ,   editStatusMessage(authoring.interactionSession().statusMessage)
    ,   selectedEdgeIndex(authoring.interactionSession().selectedEdgeIndex)
    ,   spliceTargetEdgeIndex(authoring.interactionSession().spliceTargetEdgeIndex)
    ,   editorCoordinator(
            *this,
            document,
            editorCommands,
            *this,
            *this,
            { expandedNodeId })
    ,   canvasPresentation(sceneBuilder, editorCoordinator.previewRenderer())
    ,   automation({
            *this,
            document,
            presentation,
            viewport,
            authoring,
            editorCoordinator.host(),
            editorCoordinator.previewResources()
        })
    ,   renderInvalidation(*this)
    ,   hitRouter(graph, palette, queries) {
    probeRailState.expanded = !graph.getSignalProbes().empty();
    refreshCompiledState();

    setOpaque(true);
    setWantsKeyboardFocus(true);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);
    canvasOpenGlAttached = true;
    startTimerHz(30);
}

NodeCanvas::~NodeCanvas() {
    stopTimer();
    editorCoordinator.detach();
    setCanvasOpenGlAttached(false);
}

void NodeCanvas::paint(Graphics& g) {
    const Node* expandedNode = queries.findNode(expandedNodeId);
    ignoreUnused(expandedNode);
    canvasPresentation.paint(g, presentationFrame());
}

void NodeCanvas::resized() {
    viewport.setBounds(canvasContentBounds());
    editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
    requestCanvasRepaint();
}

void NodeCanvas::visibilityChanged() {
    renderInvalidation.notifyAvailabilityChanged();
}

void NodeCanvas::focusLost(FocusChangeType) {
    probeRailState.selectedProbeId = {};
    requestCanvasRepaint();
}

void NodeCanvas::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.position;
    palette.updateHover(event.position);
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    String hovered = canvasPresentation.probeRail().probeAt(
            event.position,
            getLocalBounds().toFloat(),
            graph,
            probeRailState);
    if (hovered.isEmpty()) {
        hovered = canvasPresentation.probeRail().markerProbeAt(event.position, graph, scene);
    }
    probeRailState.hoveredProbeId = std::move(hovered);
    setMouseCursor(MouseCursor::NormalCursor);
    requestCanvasRepaint();
}

void NodeCanvas::mouseDown(const MouseEvent& event) {
    grabKeyboardFocus();
    palette.updateHover(event.position);
    editStatusMessage = {};
    lastMousePosition = event.position;
    interaction.reset();
    spliceTargetEdgeIndex = -1;
    draggingTrimeshMorph = false;
    trimeshMorphUndoPushed = false;
    draggingTrimeshVertexParameter = false;
    trimeshVertexParameterUndoPushed = false;
    activeTrimeshVertexIndex = -1;

    const Rectangle<float> workspace = getLocalBounds().toFloat();
    SignalProbeRail& probeRail = canvasPresentation.probeRail();
    if (probeRail.collapseHandleFor(workspace, probeRailState).contains(event.position)) {
        probeRailState.expanded = !probeRailState.expanded;
        resized();
        return;
    }
    if (probeRail.resizeHandleFor(workspace, probeRailState).contains(event.position)) {
        resizingProbeRail = true;
        probeRailResizeStartHeight = probeRailState.expandedHeight;
        probeRailResizeStartY = event.position.y;
        return;
    }
    const String closeProbe = probeRail.closeProbeAt(
            event.position, workspace, graph, probeRailState);
    if (closeProbe.isNotEmpty()) {
        applyAuthoringResult(authoring.removeSignalProbe(closeProbe));
        if (probeRailState.selectedProbeId == closeProbe) {
            probeRailState.selectedProbeId = {};
        }
        return;
    }
    const String railProbe = probeRail.probeAt(
            event.position, workspace, graph, probeRailState);
    if (railProbe.isNotEmpty()) {
        probeRailState.selectedProbeId = railProbe;
        requestCanvasRepaint();
        return;
    }
    probeRailState.selectedProbeId = {};
    if (probeRail.boundsFor(workspace, probeRailState).contains(event.position)) {
        requestCanvasRepaint();
        return;
    }

    if (expandedNodeId.isNotEmpty()) {
        const Node* expandedNode = queries.findNode(expandedNodeId);
        const ExpandedEditorClick click = editorCoordinator.routeClick(
                expandedNode,
                canvasContentBounds(),
                event.position);
        if (click.kind == ExpandedEditorClickKind::Close) {
            editorCoordinator.close();
        } else if (click.kind == ExpandedEditorClickKind::VoiceContextEdit) {
            applyAuthoringResult(authoring.applyVoiceContextEdit(
                    expandedNode->id,
                    *click.voiceContextEdit));
        } else if (click.kind == ExpandedEditorClickKind::TransformMode) {
            applyAuthoringResult(authoring.setTransformMode(
                    expandedNode->id,
                    *click.transformMode));
        } else if (click.kind == ExpandedEditorClickKind::Captured) {
            interaction.captureExpandedEditor();
        }

        requestCanvasRepaint();
        return;
    }

    NodeKind paletteKind;
    if (palette.findKindAt(event.position, paletteKind)) {
        const auto result = authoring.addNode(
                paletteKind,
                hitRouter.paletteCreationWorldPosition(
                        viewport,
                        getLocalBounds().toFloat(),
                        paletteKind,
                        event.position));

        if (applyAuthoringResult(result)) {
            if (const Node* node = queries.findNode(result.nodeId)) {
                interaction.beginNodeDrag(
                        node->id,
                        hitRouter.paletteDragBounds(viewport, *node, event.position));
            }
            editStatusMessage = "Node added";
            palette.close();
            requestCanvasRepaint();
        }

        return;
    }

    if (palette.findSectionAt(event.position) >= 0) {
        requestCanvasRepaint();
        return;
    }

    if (const auto action = hitRouter.nodeActionAt(viewport, event.position)) {
        switch (action->kind) {
            case CanvasNodeActionKind::CycleOperationLayout:
                if (cycleOperationPortLayout(action->nodeId)) {
                    editStatusMessage = "Port layout cycled";
                    requestCanvasRepaint();
                }
                break;

            case CanvasNodeActionKind::CycleMeshOutputSide:
                if (cycleMeshOutputSide(action->nodeId)) {
                    editStatusMessage = "Output side cycled";
                    requestCanvasRepaint();
                }
                break;

            case CanvasNodeActionKind::CycleVoiceDomain:
                if (cycleVoiceDomain(action->nodeId)) {
                    requestCanvasRepaint();
                }
                break;
        }

        return;
    }

    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    const String markerProbe = probeRail.markerProbeAt(event.position, graph, scene);
    if (markerProbe.isNotEmpty()) {
        if (event.mods.isPopupMenu()) {
            const int edgeIndex = hitRouter.edgeAt(scene, event.position);
            if (edgeIndex >= 0) {
                showEdgeMenu(edgeIndex, event.position);
            }
            return;
        }

        probeRailState.selectedProbeId = markerProbe;
        draggingProbeId = markerProbe;
        requestCanvasRepaint();
        return;
    }
    if (const auto hitPort = interaction.portAt(scene, event.position)) {
        interaction.beginConnection(*hitPort, event.position);
        selectedNodeId = hitPort->nodeId;
        selectedEdgeIndex = -1;
        requestCanvasRepaint();
        return;
    }

    const Node* hitNode = queries.findNodeAt(viewport.toWorld(event.position));

    if (hitNode != nullptr) {
        selectedNodeId = hitNode->id;
        selectedEdgeIndex = -1;
        interaction.beginNodeDrag(hitNode->id, hitNode->bounds);

        if (event.getNumberOfClicks() >= 2 && hasHostedEditor(hitNode->kind)) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
        }

        requestCanvasRepaint();
        return;
    }

    selectedEdgeIndex = hitRouter.edgeAt(scene, event.position);

    if (selectedEdgeIndex >= 0) {
        if (event.mods.isPopupMenu()) {
            showEdgeMenu(selectedEdgeIndex, event.position);
            return;
        }
        if (event.mods.isAltDown()) {
            applyAuthoringResult(authoring.toggleSignalProbe(
                    selectedEdgeIndex,
                    tapPositionForEdge(selectedEdgeIndex, event.position)));
            return;
        }

        selectedNodeId = {};
        interaction.reset();
        requestCanvasRepaint();
        return;
    }

    selectedNodeId = {};
    selectedEdgeIndex = -1;
    interaction.beginPan(viewport.getPan());
    expandedNodeId = {};

    requestCanvasRepaint();
}

void NodeCanvas::mouseDrag(const MouseEvent& event) {
    lastMousePosition = event.position;

    if (resizingProbeRail) {
        const float maximumHeight = getHeight() * 0.4f;
        probeRailState.expandedHeight = jlimit(
                SignalProbeRail::minimumExpandedHeight,
                maximumHeight,
                probeRailResizeStartHeight + probeRailResizeStartY - event.position.y);
        resized();
        return;
    }
    if (draggingProbeId.isNotEmpty()) {
        requestCanvasRepaint();
        return;
    }

    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    const auto update = interaction.drag(
            graph,
            viewport,
            scene,
            event.position,
            event.getOffsetFromDragStart().toFloat());

    if (const auto* pan = std::get_if<PanDragUpdate>(&update)) {
        viewport.setTransform(pan->pan, viewport.getZoom());
    } else if (const auto* nodeDrag = std::get_if<NodeDragUpdate>(&update)) {
        if (nodeDrag->beginTransaction) {
            authoring.beginNodeMoveGesture();
        }

        authoring.resizeNodeDuringGesture(nodeDrag->nodeId, nodeDrag->bounds);
        spliceTargetEdgeIndex = nodeDrag->moved
                ? hitRouter.spliceTargetEdgeAt(scene, event.position, nodeDrag->nodeId)
                : -1;
    }

    requestCanvasRepaint();
}

void NodeCanvas::mouseUp(const MouseEvent& event) {
    lastMousePosition = event.position;
    if (resizingProbeRail) {
        resizingProbeRail = false;
        return;
    }
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    if (draggingProbeId.isNotEmpty()) {
        const String probeId = std::move(draggingProbeId);
        draggingProbeId = {};
        const int edgeIndex = hitRouter.edgeAt(scene, event.position);
        if (edgeIndex >= 0) {
            applyAuthoringResult(authoring.reattachSignalProbe(
                    probeId,
                    edgeIndex,
                    tapPositionForEdge(edgeIndex, event.position)));
        }
        requestCanvasRepaint();
        return;
    }
    const auto completion = interaction.finish(graph, scene, event.position);

    editorCommands.endTrimeshMorphEdit();
    editorCommands.endTrimeshVertexParameterEdit();
    spliceTargetEdgeIndex = -1;

    if (const auto* nodeDrag = std::get_if<NodeDragCompletion>(&completion)) {
        if (nodeDrag->moved && spliceSelectedNodeIntoEdgeAt(event.position)) {
            authoring.commitNodeMoveGesture();
            requestCanvasRepaint();
            return;
        }

        authoring.commitNodeMoveGesture();
    } else if (const auto* connection = std::get_if<ConnectionCompletion>(&completion);
            connection != nullptr && connection->target.has_value()) {
        const auto result = authoring.connectPorts(connection->source, *connection->target);
        if (applyAuthoringResult(result)) {
            editStatusMessage = "Connected";
        } else if (result.graphEditCode == GraphEditCode::ValidationRejected) {
            editStatusMessage = "Incompatible connection";
        } else {
            editStatusMessage = "Connection cancelled";
        }
    }

    requestCanvasRepaint();
}

void NodeCanvas::mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) {
    const Rectangle<float> workspace = getLocalBounds().toFloat();
    if (probeRailState.expanded
            && SignalProbeRail::boundsFor(workspace, probeRailState).contains(event.position)) {
        const float wheelDelta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY)
                ? wheel.deltaX
                : wheel.deltaY;
        probeRailState.horizontalOffset = jlimit(
                0.f,
                SignalProbeRail::maximumHorizontalOffset(
                        workspace,
                        (int) graph.getSignalProbes().size()),
                probeRailState.horizontalOffset - wheelDelta * 420.f);
        requestCanvasRepaint();
        return;
    }

    const Node* expandedNode = queries.findNode(expandedNodeId);
    if (expandedNode != nullptr
            && editorCoordinator.boundsFor(expandedNode, canvasContentBounds())
                    .contains(event.position)) {
        requestCanvasRepaint();
        return;
    }

    constexpr float panScale = 720.f;
    viewport.panBy(Point<float>(wheel.deltaX * panScale, wheel.deltaY * panScale));
    requestCanvasRepaint();
}

void NodeCanvas::mouseMagnify(const MouseEvent& event, float scaleFactor) {
    viewport.zoomAround(event.position, scaleFactor);
    requestCanvasRepaint();
}

bool NodeCanvas::keyPressed(const KeyPress& key) {
    const bool commandDown = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown();
    const int keyCode = key.getKeyCode();
    const juce_wchar keyChar = CharacterFunctions::toLowerCase(key.getTextCharacter());

    if (commandDown && (keyChar == 'z' || keyCode == 'z' || keyCode == 'Z')) {
        return key.getModifiers().isShiftDown() ? redo() : undo();
    }

    if (commandDown && (keyChar == 'y' || keyCode == 'y' || keyCode == 'Y')) {
        return redo();
    }

    if (key == KeyPress::escapeKey) {
        return clearSelection();
    }

    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey) {
        if (selectedEdgeIndex >= 0) {
            if (!applyAuthoringResult(authoring.deleteEdge(selectedEdgeIndex))) {
                return false;
            }
            editStatusMessage = "Edge deleted";
            requestCanvasRepaint();
            return true;
        }

        if (selectedNodeId.isEmpty()) {
            return false;
        }

        if (!applyAuthoringResult(authoring.deleteNode(selectedNodeId))) {
            return false;
        }
        editStatusMessage = "Node deleted";
        requestCanvasRepaint();
        return true;
    }

    return false;
}

void NodeCanvas::newOpenGLContextCreated() {
    renderer.initialize();
}

void NodeCanvas::renderOpenGL() {
    if (kUseGlCanvasUnderlay) {
        gl::glDisable(gl::GL_SCISSOR_TEST);
        OpenGLHelpers::clear(kCanvasBackground);
        canvasPresentation.renderOpenGL(
                renderer,
                presentationFrame(),
                (float) openGLContext.getRenderingScale());
        editorCoordinator.renderOpenGL((float) openGLContext.getRenderingScale());
    } else {
        OpenGLHelpers::clear(kCanvasBackground);
    }
}

void NodeCanvas::openGLContextClosing() {
    editorCoordinator.releaseOpenGLResources();

    renderer.shutdown();
}

void NodeCanvas::timerCallback() {
    if (compiledStateRefreshPending
            && (int32) (Time::getMillisecondCounter() - compiledStateRefreshDueMs) >= 0) {
        flushScheduledCompiledStateRefresh();
    }

    editorCoordinator.syncEffectNodes(graph);
    editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());

    const auto mouse = getMouseXYRelative().toFloat();
    const int previousPaletteSectionIndex = palette.activeSection();

    if (getLocalBounds().toFloat().contains(mouse)) {
        palette.updateHover(mouse);
    }

    if (getLocalBounds().toFloat().contains(mouse)
            && (mouse != lastMousePosition || previousPaletteSectionIndex != palette.activeSection())) {
        lastMousePosition = mouse;
        requestCanvasRepaint();
    }
}

void NodeCanvas::setCanvasOpenGlAttached(bool shouldAttach) {
    if (canvasOpenGlAttached == shouldAttach) {
        return;
    }

    if (shouldAttach) {
        openGLContext.attachTo(*this);
    } else {
        openGLContext.detach();
    }

    canvasOpenGlAttached = shouldAttach;
}

NodeCanvasPresentationFrame NodeCanvas::presentationFrame() const {
    const Rectangle<float> workspace = getLocalBounds().toFloat();
    const Rectangle<float> content = canvasContentBounds();
    const Node* expandedNode = queries.findNode(expandedNodeId);
    const Rectangle<float> occlusion = editorCoordinator.blocksCanvas(expandedNode)
            ? editorCoordinator.boundsFor(expandedNode, content)
            : Rectangle<float> {};
    std::optional<PendingConnectionPresentation> pending;
    if (const auto* connection = std::get_if<PortConnectionGesture>(&interaction.gesture())) {
        pending = PendingConnectionPresentation { connection->source, connection->endpoint };
    }

    SnapGuidePresentation snapGuides;
    if (const auto* drag = std::get_if<NodeDragGesture>(&interaction.gesture())) {
        snapGuides = {
                drag->guides.x.has_value(),
                drag->guides.y.has_value(),
                drag->guides.x.value_or(0.f),
                drag->guides.y.value_or(0.f)
        };
    }
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());

    return {
            graph,
            compileResult,
            previewResult,
            viewport,
            palette,
            content,
            occlusion,
            lastMousePosition,
            selectedNodeId,
            editStatusMessage,
            hitRouter.hoverTextFor(viewport, scene, lastMousePosition),
            std::move(pending),
            snapGuides,
            presentation.revision(),
            document.revision(),
            selectedEdgeIndex,
            spliceTargetEdgeIndex,
            kUseGlCanvasUnderlay,
            workspace,
            probeRailState
    };
}

Point<float> NodeCanvas::viewportCentreWorld() const {
    viewport.setBounds(canvasContentBounds());
    return viewport.centreWorld();
}

Rectangle<float> NodeCanvas::canvasContentBounds() const {
    return SignalProbeRail::contentBoundsFor(getLocalBounds().toFloat(), probeRailState);
}

float NodeCanvas::tapPositionForEdge(int edgeIndex, Point<float> screenPosition) const {
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    for (const auto& edge : scene.edges) {
        if (edge.edgeIndex != edgeIndex) {
            continue;
        }

        Point<float> nearest;
        const float distance = edge.cablePath.getNearestPoint(screenPosition, nearest);
        const float length = edge.cablePath.getLength();
        return length > 0.f ? jlimit(0.f, 1.f, distance / length) : 0.5f;
    }
    return 0.5f;
}

void NodeCanvas::showEdgeMenu(int edgeIndex, Point<float> screenPosition) {
    if (edgeIndex < 0 || edgeIndex >= (int) graph.getEdges().size()) {
        return;
    }
    const Edge edge = graph.getEdges()[(size_t) edgeIndex];
    const bool spying = graph.findSignalProbeForSource(edge.sourceNodeId, edge.sourcePortId) != nullptr;
    const float tapPosition = tapPositionForEdge(edgeIndex, screenPosition);

    PopupMenu menu;
    menu.addItem(1, spying ? "Stop Spying" : "Spy on Signal");
    menu.addSeparator();
    menu.addItem(2, "Delete Cable");
    menu.showMenuAsync(
            PopupMenu::Options()
                    .withTargetComponent(this)
                    .withMousePosition(),
            [safeThis = SafePointer<NodeCanvas>(this), edge, tapPosition](int result) {
                if (safeThis == nullptr) {
                    return;
                }

                const auto& edges = safeThis->graph.getEdges();
                const auto found = std::find_if(edges.begin(), edges.end(), [&](const auto& candidate) {
                    return candidate.sourceNodeId == edge.sourceNodeId
                            && candidate.sourcePortId == edge.sourcePortId
                            && candidate.destNodeId == edge.destNodeId
                            && candidate.destPortId == edge.destPortId;
                });
                if (found == edges.end()) {
                    return;
                }
                const int currentEdgeIndex = (int) std::distance(edges.begin(), found);

                if (result == 1) {
                    safeThis->applyAuthoringResult(
                            safeThis->authoring.toggleSignalProbe(currentEdgeIndex, tapPosition));
                } else if (result == 2) {
                    safeThis->applyAuthoringResult(safeThis->authoring.deleteEdge(currentEdgeIndex));
                }
            });
}

void NodeCanvas::refreshCompiledState() {
    compiledStateRefreshPending = false;
    editorCoordinator.clearPreviewCache();
    presentation.refresh(graph, document.revision(), document.lastChange());
}

bool NodeCanvas::applyAuthoringResult(const NodeCanvasAuthoringResult& result) {
    if (!result.handled) {
        return false;
    }

    if (result.graphChanged) {
        compiledStateRefreshPending = false;
        editorCoordinator.clearPreviewCache();

        if (document.lastChange().probesChanged) {
            probeRailState.expanded = !graph.getSignalProbes().empty();
            probeRailState.horizontalOffset = jmin(
                    probeRailState.horizontalOffset,
                    SignalProbeRail::maximumHorizontalOffset(
                            getLocalBounds().toFloat(),
                            (int) graph.getSignalProbes().size()));
            resized();
        }
    }
    if (result.effects.resetInteraction) {
        interaction.reset();
        spliceTargetEdgeIndex = -1;
    }
    if (result.effects.editorBindingChanged) {
        editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
    }
    if (result.effects.repaintRequested) {
        requestCanvasRepaint();
    }

    return result.succeeded;
}

NodeCanvasAutomationPresentation NodeCanvas::automationPresentationState() const {
    return {
            selectedNodeId,
            expandedNodeId,
            editStatusMessage,
            selectedEdgeIndex
    };
}

void NodeCanvas::scheduleCompiledStateRefresh() {
    constexpr uint32 refreshDelayMs = 55;

    if (compiledStateRefreshPending) {
        return;
    }

    compiledStateRefreshPending = true;
    compiledStateRefreshDueMs = Time::getMillisecondCounter() + refreshDelayMs;
}

void NodeCanvas::flushScheduledCompiledStateRefresh() {
    if (!compiledStateRefreshPending) {
        return;
    }

    refreshCompiledState();
    openGLContext.triggerRepaint();
    requestCanvasRepaint();
}

var NodeCanvas::exportAutomationState() const {
    return automation.exportState(automationPresentationState());
}

String NodeCanvas::exportGraphXml() const {
    return automation.exportGraphXml();
}

bool NodeCanvas::openNodeEditorForAutomation(const String& nodeId) {
    return applyAuthoringResult(automation.openEditor(nodeId));
}

bool NodeCanvas::addNodeForAutomation(const String& kindId, Point<float> position, String& nodeId) {
    const auto result = automation.addNode(kindId, position);
    if (!applyAuthoringResult(result)) {
        return false;
    }

    nodeId = result.nodeId;
    return true;
}

bool NodeCanvas::moveNodeForAutomation(const String& nodeId, Point<float> position) {
    return applyAuthoringResult(automation.moveNode(nodeId, position));
}

bool NodeCanvas::connectPortsForAutomation(
        const String& sourceNodeId,
        const String& sourcePortId,
        const String& destNodeId,
        const String& destPortId) {
    return applyAuthoringResult(automation.connectPorts(
            sourceNodeId,
            sourcePortId,
            destNodeId,
            destPortId));
}

bool NodeCanvas::deleteNodeForAutomation(const String& nodeId) {
    return applyAuthoringResult(automation.deleteNode(nodeId));
}

bool NodeCanvas::deleteEdgeForAutomation(int edgeIndex) {
    return applyAuthoringResult(automation.deleteEdge(edgeIndex));
}

bool NodeCanvas::setNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    return applyAuthoringResult(automation.setNodeParameter(nodeId, parameterId, label, value));
}

bool NodeCanvas::setMorphSliderForAutomation(const String& nodeId, const String& axis, float value) {
    return applyAuthoringResult(automation.setMorph(nodeId, axis, value));
}

bool NodeCanvas::setPrimaryAxisForAutomation(const String& nodeId, const String& axis) {
    return applyAuthoringResult(automation.setPrimaryAxis(nodeId, axis));
}

bool NodeCanvas::toggleLinkForAutomation(const String& nodeId, const String& axis) {
    return applyAuthoringResult(automation.toggleLink(nodeId, axis));
}

bool NodeCanvas::selectVertexForAutomation(const String& nodeId, int vertexIndex) {
    return applyAuthoringResult(automation.selectVertex(nodeId, vertexIndex));
}

bool NodeCanvas::setVertexParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        float value) {
    return applyAuthoringResult(automation.setVertexParameter(nodeId, parameterId, value));
}

bool NodeCanvas::getNodeParameterForAutomation(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    return automation.getNodeParameter(nodeId, parameterId, value);
}

var NodeCanvas::inspectNodeControlsForAutomation(const String& nodeId) const {
    return automation.inspectNodeControls(nodeId, automationPresentationState());
}

var NodeCanvas::inspectPointerTargetsForAutomation() const {
    return automation.inspectPointerTargets(automationPresentationState());
}

var NodeCanvas::inspectOpenGLDiagnosticsForAutomation() const {
    return automation.inspectOpenGLDiagnostics({ canvasOpenGlAttached, expandedNodeId });
}

var NodeCanvas::captureAudioForAutomation(size_t frameCount) const {
    return automation.captureAudio(frameCount);
}

File NodeCanvas::snapshotFile() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("CycleV2")
            .getChildFile("graph-snapshot.xml");
}

bool NodeCanvas::saveGraphToFile(const File& file) {
    return applyAuthoringResult(authoring.saveGraph(file));
}

bool NodeCanvas::loadGraphFromFile(const File& file) {
    const bool loaded = applyAuthoringResult(authoring.loadGraph(file));
    if (loaded) {
        probeRailState.expanded = !graph.getSignalProbes().empty();
        probeRailState.horizontalOffset = 0.f;
        resized();
    }
    return loaded;
}

bool NodeCanvas::saveSnapshot() {
    const auto result = authoring.saveSnapshot(snapshotFile());
    applyAuthoringResult(result);
    return result.handled;
}

bool NodeCanvas::loadSnapshot() {
    const auto result = authoring.loadSnapshot(snapshotFile());
    applyAuthoringResult(result);
    return result.handled;
}

bool NodeCanvas::undo() {
    const auto result = authoring.undo();
    applyAuthoringResult(result);
    return result.handled;
}

bool NodeCanvas::redo() {
    const auto result = authoring.redo();
    applyAuthoringResult(result);
    return result.handled;
}

bool NodeCanvas::spliceSelectedNodeIntoEdgeAt(Point<float> screenPosition) {
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    const int edgeIndex = hitRouter.spliceTargetEdgeAt(scene, screenPosition, selectedNodeId);
    return applyAuthoringResult(authoring.spliceSelectedNodeIntoEdge(edgeIndex));
}

bool NodeCanvas::clearSelection() {
    const bool cleared = authoring.clearSelection();
    if (cleared) {
        editorCoordinator.updateHost(nullptr, canvasContentBounds());
        requestCanvasRepaint();
    }
    return cleared;
}

bool NodeCanvas::cycleOperationPortLayout(const String& nodeId) {
    return applyAuthoringResult(authoring.cycleOperationPortLayout(nodeId));
}

bool NodeCanvas::cycleMeshOutputSide(const String& nodeId) {
    return applyAuthoringResult(authoring.cycleMeshOutputSide(nodeId));
}

bool NodeCanvas::cycleVoiceDomain(const String& nodeId) {
    return applyAuthoringResult(authoring.cycleVoiceDomain(nodeId));
}

void NodeCanvas::closeNodeEditor() {
    editorCoordinator.close();
}

void NodeCanvas::repaintNodeEditor(bool openGl) {
    if (openGl) {
        openGLContext.triggerRepaint();
    }
    requestCanvasRepaint();
}

void NodeCanvas::selectEditedNode(const String& nodeId) {
    selectedNodeId = nodeId;
    selectedEdgeIndex = -1;
}

void NodeCanvas::setNodeEditorStatus(const String& message) {
    editStatusMessage = message;
}

void NodeCanvas::scheduleNodeEditorRefresh() {
    scheduleCompiledStateRefresh();
}

void NodeCanvas::flushNodeEditorRefresh() {
    flushScheduledCompiledStateRefresh();
}

void NodeCanvas::refreshNodeEditorPresentation() {
    refreshCompiledState();
}

Point<float> NodeCanvas::nodeEditorCreationPosition() const {
    return viewportCentreWorld();
}

void NodeCanvas::rebindNodeEditor() {
    editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
}

Effect2DWidget* NodeCanvas::effect2DWidget(const Node& node) {
    return &editorCoordinator.previewResources().effect2DWidget(node);
}

TrimeshWidget* NodeCanvas::trimeshWidget(const Node& node) {
    return &editorCoordinator.previewResources().trimeshWidget(node.id);
}

TrimeshRenderProfile NodeCanvas::trimeshRenderProfile(const Node& node) const {
    return queries.renderProfileForNodeOutput(node, "out");
}

std::array<String, 6> NodeCanvas::trimeshGuideLabels(const Node& node) {
    return editorCoordinator.trimeshGuideLabelsFor(node);
}

void NodeCanvas::paintNodePreview(
        Graphics& graphics,
        const Node& node,
        Rectangle<float> bounds) {
    const NodePreviewResult* result = queries.findPreviewResult(node.id);
    editorCoordinator.previewRenderer().paint(graphics, {
            node,
            result,
            bounds,
            result != nullptr
                    ? TrimeshRenderProfile::fromDomain(result->domain)
                    : TrimeshRenderProfile::fromDomain(PortDomain::ControlSignal),
            1.f,
            true
    });
}

void NodeCanvas::requestCanvasRepaint() {
    renderInvalidation.request(NodeCanvasInvalidation::CanvasRepaint);
}

uint32_t NodeCanvas::availableRenderInvalidations() const {
    return isShowing() ? NodeCanvasInvalidation::CanvasRepaint : 0;
}

void NodeCanvas::flushRenderInvalidations(uint32_t categories) {
    if ((categories & NodeCanvasInvalidation::CanvasRepaint) != 0) {
        Component::repaint();
    }
}

}
