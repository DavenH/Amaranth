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

bool isPreviewableNode(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().previewable;
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
    viewport.setBounds(getLocalBounds().toFloat());
    editorCoordinator.updateHost(queries.findNode(expandedNodeId));
    requestCanvasRepaint();
}

void NodeCanvas::visibilityChanged() {
    renderInvalidation.notifyAvailabilityChanged();
}

void NodeCanvas::mouseMove(const MouseEvent& event) {
    lastMousePosition = event.position;
    palette.updateHover(event.position);
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

    if (expandedNodeId.isNotEmpty()) {
        const Node* expandedNode = queries.findNode(expandedNodeId);
        const ExpandedEditorClick click = editorCoordinator.routeClick(
                expandedNode,
                getLocalBounds().toFloat(),
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

        if (event.getNumberOfClicks() >= 2 && isPreviewableNode(hitNode->kind)) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
        }

        requestCanvasRepaint();
        return;
    }

    selectedEdgeIndex = hitRouter.edgeAt(scene, event.position);

    if (selectedEdgeIndex >= 0) {
        if (event.mods.isCtrlDown()) {
            if (applyAuthoringResult(authoring.deleteEdge(selectedEdgeIndex))) {
                editStatusMessage = "Edge cut";
            }

            requestCanvasRepaint();
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
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
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
    const Node* expandedNode = queries.findNode(expandedNodeId);
    if (expandedNode != nullptr
            && editorCoordinator.boundsFor(expandedNode, getLocalBounds().toFloat())
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
    editorCoordinator.updateHost(queries.findNode(expandedNodeId));

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
    const Node* expandedNode = queries.findNode(expandedNodeId);
    const Rectangle<float> occlusion = editorCoordinator.blocksCanvas(expandedNode)
            ? editorCoordinator.boundsFor(expandedNode, getLocalBounds().toFloat())
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
            getLocalBounds().toFloat(),
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
            kUseGlCanvasUnderlay
    };
}

Point<float> NodeCanvas::viewportCentreWorld() const {
    viewport.setBounds(getLocalBounds().toFloat());
    return viewport.centreWorld();
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
    }
    if (result.effects.resetInteraction) {
        interaction.reset();
        spliceTargetEdgeIndex = -1;
    }
    if (result.effects.editorBindingChanged) {
        editorCoordinator.updateHost(queries.findNode(expandedNodeId));
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
    return applyAuthoringResult(authoring.loadGraph(file));
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
        editorCoordinator.updateHost(nullptr);
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
    editorCoordinator.updateHost(queries.findNode(expandedNodeId));
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
