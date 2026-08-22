#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <iterator>
#include <limits>
#include <utility>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "NodeCanvas.h"

#include "../Graph/NodeParameterMap.h"
#include "NodeViewModule.h"
#include "TransformCompactEditor.h"

#include "../Runtime/GraphAudioExecutor.h"

namespace CycleV2 {

namespace NodeCanvasInvalidation {

constexpr uint32_t CanvasRepaint = 1u << 0;

}

namespace {

const Colour kCanvasBackground { 0xff101318 };
const Colour kCanvasGridMajor  { 0x2f5b6370 };
const Colour kCanvasGridMinor  { 0x182f363f };
constexpr bool kUseGlCanvasUnderlay = true;

bool hasExpandedEditor(NodeKind kind) {
    return NodeViewModuleRegistry::instance().moduleFor(kind).capabilities().expandedEditor;
}

Rectangle<float> inlinePanDialBounds(
        const NodeCanvasViewport& viewport,
        const Node& node) {
    return viewport.toScreen(node.bounds).reduced(12.f * viewport.getZoom());
}

Rectangle<float> inlinePanHitBounds(
        const NodeCanvasViewport& viewport,
        const Node& node) {
    return viewport.toScreen(node.bounds).expanded(3.f * viewport.getZoom());
}

bool inlinePanContains(
        const NodeCanvasViewport& viewport,
        const Node& node,
        Point<float> position) {
    const Rectangle<float> hitBounds = inlinePanHitBounds(viewport, node);
    const float radius = jmin(hitBounds.getWidth(), hitBounds.getHeight()) * 0.5f;
    return hitBounds.getCentre().getDistanceSquaredFrom(position) <= radius * radius;
}

const Node* findInlinePanAt(
        const NodeGraph& graph,
        const NodeCanvasViewport& viewport,
        Point<float> position) {
    const auto& nodes = graph.getNodes();
    for (auto node = nodes.rbegin(); node != nodes.rend(); ++node) {
        if (node->kind == NodeKind::SpectralLayer
                && inlinePanContains(viewport, *node, position)) {
            return &*node;
        }
    }

    return nullptr;
}

bool inlinePanDialContains(
        const NodeCanvasViewport& viewport,
        const Node& node,
        Point<float> position) {
    const Rectangle<float> dial = inlinePanDialBounds(viewport, node);
    const float radius = jmin(dial.getWidth(), dial.getHeight()) * 0.5f;
    return dial.getCentre().getDistanceSquaredFrom(position) <= radius * radius;
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
        settings(nullptr)
    ,   document(createStartupDocument())
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
    settings.initialiseSettings();
    probeRailState.refreshMode = settings.getGlobalSettingValue(
            AppSettings::ProbeEditRefreshPolicy) == 1
            ? ProbeRefreshMode::LiveLatest
            : ProbeRefreshMode::OnGestureCommit;
    dockSplitRatio = jlimit(
            0.2f,
            0.8f,
            settings.getGlobalSettingValue(AppSettings::GuideDockSplitPercent) / 100.f);
    guideShelfState.minimized = settings.getGlobalSettingValue(
            AppSettings::GuideShelfMinimized) != 0;
    probeRailState.minimized = settings.getGlobalSettingValue(
            AppSettings::SpyShelfMinimized) != 0;
    globalUnisonPreviewContext.voiceDurationSeconds = jlimit(
            CycleDsp::voiceLengthSeconds(0.f),
            CycleDsp::voiceLengthSeconds(1.f),
            settings.getGlobalSettingValue(
                    AppSettings::PreviewVoiceLengthMilliseconds) / 1000.0);
    probeRailState.expanded = settings.getGlobalSettingValue(AppSettings::GuideSpyDockExpanded) != 0;
    refreshCompiledState();

    setOpaque(true);
    setName("NodeCanvas");
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
    canvasPresentation.paint(g, presentationFrame());
    if (canvasPresentation.guideShelfNeedsOpenGLPreviewRender()) {
        openGLContext.triggerRepaint();
    }
    if (expandedNode != nullptr && expandedNode->kind == NodeKind::VoiceContext) {
        VoiceContextCompactEditor::paintExpanded(
                g,
                editorCoordinator.boundsFor(expandedNode, canvasContentBounds()),
                *expandedNode,
                globalUnisonPreviewContext.voiceDurationSeconds);
    }
}

void NodeCanvas::resized() {
    viewport.setBounds(canvasContentBounds());
    if (guideEditor != nullptr && guideEditor->isVisible()) {
        guideEditor->setBounds(canvasContentBounds().reduced(36.f, 24.f).toNearestInt());
    }
    editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
    notifyOverlayPresentationChanged();
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
    updateHoverAt(event.position);
    requestCanvasRepaint();
}

void NodeCanvas::updateHoverAt(Point<float> position) {
    lastMousePosition = position;
    palette.updateHover(position);
    const auto& scene = sceneBuilder.build(
            graph,
            viewport,
            presentation.revision(),
            document.revision());
    String hovered = canvasPresentation.probeRail().probeAt(
            position,
            GuideCurveShelf::spyWorkspace(
                    getLocalBounds().toFloat(),
                    dockSplitRatio,
                    guideShelfState.minimized,
                    probeRailState.minimized),
            graph,
            probeRailState);
    if (hovered.isEmpty()) {
        hovered = canvasPresentation.probeRail().markerProbeAt(position, graph, scene);
    }
    probeRailState.hoveredProbeId = std::move(hovered);

    const Node* inlinePan = findInlinePanAt(graph, viewport, position);
    MouseCursor cursor = MouseCursor::NormalCursor;
    if (inlinePan != nullptr && inlinePan->kind == NodeKind::SpectralLayer) {
        cursor = inlinePanDialContains(viewport, *inlinePan, position)
                ? MouseCursor::UpDownResizeCursor
                : MouseCursor::UpDownLeftRightResizeCursor;
    }
    setMouseCursor(cursor);
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
    draggingVoiceContextSlider.reset();
    draggingSpectralPanNodeId = {};
    trimeshVertexParameterUndoPushed = false;
    activeTrimeshVertexIndex = -1;

    const Rectangle<float> workspace = getLocalBounds().toFloat();
    SignalProbeRail& probeRail = canvasPresentation.probeRail();
    if (!probeRailState.expanded
            && SignalProbeRail::boundsFor(workspace, probeRailState).contains(event.position)) {
        probeRailState.expanded = true;
        settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = true;
        resized();
        return;
    }
    const Rectangle<float> spyWorkspace = GuideCurveShelf::spyWorkspace(
            workspace,
            dockSplitRatio,
            guideShelfState.minimized,
            probeRailState.minimized);
    const Rectangle<float> guideShelf = GuideCurveShelf::boundsFor(
            workspace,
            probeRailState,
            dockSplitRatio,
            guideShelfState);
    if (guideShelfState.minimized && guideShelf.contains(event.position)) {
        guideShelfState.minimized = false;
        settings.getGlobalSetting(AppSettings::GuideShelfMinimized) = false;
        requestCanvasRepaint();
        return;
    }
    if (probeRailState.minimized
            && SignalProbeRail::boundsFor(spyWorkspace, probeRailState).contains(event.position)) {
        probeRailState.minimized = false;
        settings.getGlobalSetting(AppSettings::SpyShelfMinimized) = false;
        requestCanvasRepaint();
        return;
    }
    const float dividerX = GuideCurveShelf::guideWorkspace(
            workspace,
            dockSplitRatio,
            guideShelfState.minimized,
            probeRailState.minimized).getRight();
    if (!guideShelfState.minimized && !probeRailState.minimized
            && Rectangle<float>(dividerX - 4.f, guideShelf.getY(), 8.f, guideShelf.getHeight())
                .contains(event.position)) {
        resizingDockSplit = true;
        return;
    }
    if (GuideCurveShelf::addButtonBounds(
                workspace,
                probeRailState,
                dockSplitRatio,
                guideShelfState).contains(event.position)) {
        const GraphEditResult result = commands.createGuideCurve();
        if (result.succeeded()) {
            guideShelfState.selectedGuideId = result.nodeId;
            editStatusMessage = "Guide Curve created";
        }
        requestCanvasRepaint();
        return;
    }
    if (GuideCurveShelf::minimizeButtonBounds(
                workspace,
                probeRailState,
                dockSplitRatio,
                guideShelfState).contains(event.position)) {
        guideShelfState.minimized = true;
        settings.getGlobalSetting(AppSettings::GuideShelfMinimized) = true;
        if (probeRailState.minimized) {
            probeRailState.expanded = false;
            settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = false;
            resized();
        }
        requestCanvasRepaint();
        return;
    }
    const String selectedGuide = GuideCurveShelf::guideAt(
            event.position,
            graph,
            workspace,
            probeRailState,
            dockSplitRatio,
            guideShelfState);
    if (selectedGuide.isNotEmpty()) {
        if (event.mods.isPopupMenu()) {
            const GuideCurveResource* guide = graph.findGuideCurve(selectedGuide);
            if (guide == nullptr) {
                return;
            }
            const int assignmentCount = (int) std::count_if(
                    graph.getGuideAssignments().begin(),
                    graph.getGuideAssignments().end(),
                    [&](const GuideCurveAssignment& assignment) {
                        return assignment.guideId == selectedGuide;
                    });
            AlertWindow dialog(
                    "Guide Curve",
                    "Rename this resource, or delete it and detach "
                            + String(assignmentCount) + " assignment"
                            + (assignmentCount == 1 ? "." : "s."),
                    AlertWindow::NoIcon,
                    this);
            dialog.addTextEditor("name", guide->name, "Name:");
            dialog.addButton("Rename", 1);
            dialog.addButton("Delete", 2);
            dialog.addButton("Duplicate", 3);
            dialog.addButton("Move Left", 4);
            dialog.addButton("Move Right", 5);
            dialog.addButton("Cancel", 0);
            const int action = dialog.runModalLoop();
            if (action == 1) {
                if (commands.renameGuideCurve(
                            selectedGuide,
                            dialog.getTextEditorContents("name")).succeeded()) {
                    editStatusMessage = "Guide Curve renamed";
                }
            } else if (action == 2 && commands.removeGuideCurve(selectedGuide).succeeded()) {
                guideShelfState.selectedGuideId = {};
                editStatusMessage = "Guide Curve deleted";
            } else if (action == 3) {
                const GraphEditResult duplicated = commands.duplicateGuideCurve(selectedGuide);
                if (duplicated.succeeded()) {
                    guideShelfState.selectedGuideId = duplicated.nodeId;
                    editStatusMessage = "Guide Curve duplicated";
                }
            } else if (action == 4 || action == 5) {
                const int direction = action == 4 ? -1 : 1;
                if (commands.reorderGuideCurve(
                            selectedGuide,
                            guide->shelfOrder + direction).succeeded()) {
                    editStatusMessage = "Guide Curve reordered";
                }
            }
            requestCanvasRepaint();
            return;
        }
        guideShelfState.selectedGuideId = selectedGuide;
        if (event.getNumberOfClicks() >= 2) {
            openGuideEditor(selectedGuide);
        }
        requestCanvasRepaint();
        return;
    }
    if (guideShelf.contains(event.position)) {
        requestCanvasRepaint();
        return;
    }
    if (probeDetailState.isOpen()) {
        const Rectangle<float> detail = SignalProbeDetailView::boundsFor(canvasContentBounds());
        if (SignalProbeDetailView::closeBounds(detail).contains(event.position)) {
            probeDetailState.close();
            notifyOverlayPresentationChanged();
            requestCanvasRepaint();
            return;
        }
        if (detail.contains(event.position)) {
            return;
        }
    }
    if (probeRail.refreshModeBoundsFor(spyWorkspace, probeRailState).contains(event.position)) {
        probeRailState.refreshMode = probeRailState.refreshMode == ProbeRefreshMode::LiveLatest
                ? ProbeRefreshMode::OnGestureCommit
                : ProbeRefreshMode::LiveLatest;
        settings.getGlobalSetting(AppSettings::ProbeEditRefreshPolicy) =
                probeRailState.refreshMode == ProbeRefreshMode::LiveLatest ? 1 : 0;
        requestCanvasRepaint();
        return;
    }
    if (probeRail.minimizeButtonBoundsFor(spyWorkspace, probeRailState).contains(event.position)) {
        probeRailState.minimized = true;
        settings.getGlobalSetting(AppSettings::SpyShelfMinimized) = true;
        if (guideShelfState.minimized) {
            probeRailState.expanded = false;
            settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = false;
            resized();
        }
        requestCanvasRepaint();
        return;
    }
    if (probeRail.collapseHandleFor(spyWorkspace, probeRailState).contains(event.position)) {
        probeRailState.expanded = !probeRailState.expanded;
        settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = probeRailState.expanded;
        if (!probeRailState.expanded) {
            probeDetailState.close();
            notifyOverlayPresentationChanged();
        }
        resized();
        return;
    }
    if (probeRail.resizeHandleFor(spyWorkspace, probeRailState).contains(event.position)) {
        resizingProbeRail = true;
        probeRailResizeStartHeight = probeRailState.expandedHeight;
        probeRailResizeStartY = event.position.y;
        return;
    }
    const String closeProbe = probeRail.closeProbeAt(
            event.position, spyWorkspace, graph, probeRailState);
    if (closeProbe.isNotEmpty()) {
        applyAuthoringResult(authoring.removeSignalProbe(closeProbe));
        if (probeRailState.selectedProbeId == closeProbe) {
            probeRailState.selectedProbeId = {};
        }
        if (probeDetailState.probeId == closeProbe) {
            probeDetailState.close();
            notifyOverlayPresentationChanged();
        }
        return;
    }
    const String railProbe = probeRail.probeAt(
            event.position, spyWorkspace, graph, probeRailState);
    if (railProbe.isNotEmpty()) {
        probeRailState.selectedProbeId = railProbe;
        if (event.getNumberOfClicks() >= 2) {
            openProbeDetail(railProbe);
        }
        requestCanvasRepaint();
        return;
    }
    probeRailState.selectedProbeId = {};
    if (probeRail.boundsFor(spyWorkspace, probeRailState).contains(event.position)) {
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
            notifyOverlayPresentationChanged();
        } else if (click.kind == ExpandedEditorClickKind::VoiceContextEdit) {
            const auto control = click.voiceContextEdit->control;
            if (control == VoiceContextEdit::Control::VoiceLength) {
                draggingVoiceContextSlider = control;
                setPreviewVoiceLength(click.voiceContextEdit->value.getDoubleValue());
            } else if (control == VoiceContextEdit::Control::Octave
                    || control == VoiceContextEdit::Control::Pitch
                    || control == VoiceContextEdit::Control::Oversampling) {
                if (authoring.beginVoiceContextSliderGesture(
                            expandedNode->id,
                            *click.voiceContextEdit)) {
                    draggingVoiceContextSlider = control;
                }
            } else {
                applyAuthoringResult(authoring.applyVoiceContextEdit(
                        expandedNode->id,
                        *click.voiceContextEdit));
            }
        } else if (click.kind == ExpandedEditorClickKind::TransformMode) {
            applyAuthoringResult(authoring.setTransformMode(
                    expandedNode->id,
                    *click.transformMode));
        } else if (click.kind == ExpandedEditorClickKind::Captured) {
            interaction.captureExpandedEditor();
        }

        if (click.kind != ExpandedEditorClickKind::Unclaimed) {
            requestCanvasRepaint();
            return;
        }
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
    const Node* inlinePan = findInlinePanAt(graph, viewport, event.position);
    if (inlinePan != nullptr && inlinePan->kind == NodeKind::SpectralLayer) {
        if (inlinePanDialContains(viewport, *inlinePan, event.position)
                && authoring.beginSpectralPanGesture(inlinePan->id)) {
            draggingSpectralPanNodeId = inlinePan->id;
            spectralPanDragStartValue = NodeParameterMap(*inlinePan)
                    .floatValue("pan", 0.5f);
            selectedNodeId = inlinePan->id;
            selectedEdgeIndex = -1;
            requestCanvasRepaint();
            return;
        }
    }
    if (const auto hitPort = interaction.portAt(scene, event.position)) {
        interaction.beginConnection(*hitPort, event.position);
        selectedNodeId = hitPort->nodeId;
        selectedEdgeIndex = -1;
        requestCanvasRepaint();
        return;
    }

    const Node* hitNode = queries.findNodeAt(viewport.toWorld(event.position));
    if (hitNode == nullptr && inlinePan != nullptr) {
        hitNode = inlinePan;
    }

    if (hitNode != nullptr) {
        selectedNodeId = hitNode->id;
        selectedEdgeIndex = -1;
        interaction.beginNodeDrag(hitNode->id, hitNode->bounds);

        if (event.getNumberOfClicks() >= 2 && hasExpandedEditor(hitNode->kind)) {
            expandedNodeId = expandedNodeId == hitNode->id ? String() : hitNode->id;
            editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
            notifyOverlayPresentationChanged();
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

    if (draggingSpectralPanNodeId.isNotEmpty()) {
        const float value = jlimit(
                0.f,
                1.f,
                spectralPanDragStartValue
                        - event.getOffsetFromDragStart().y / 120.f);
        authoring.updateSpectralPanGesture(value);
        requestCanvasRepaint();
        return;
    }

    if (draggingVoiceContextSlider.has_value()) {
        const Node* expandedNode = queries.findNode(expandedNodeId);
        if (expandedNode != nullptr && expandedNode->kind == NodeKind::VoiceContext) {
            const auto edit = VoiceContextCompactEditor::sliderEditAt(
                    *draggingVoiceContextSlider,
                    editorCoordinator.boundsFor(expandedNode, canvasContentBounds()),
                    event.position.x);
            if (edit.has_value()) {
                if (edit->control == VoiceContextEdit::Control::VoiceLength) {
                    setPreviewVoiceLength(edit->value.getDoubleValue());
                } else {
                    authoring.updateVoiceContextSliderGesture(*edit);
                    requestCanvasRepaint();
                }
            }
        }
        return;
    }

    if (resizingProbeRail) {
        const float maximumHeight = getHeight() * 0.4f;
        probeRailState.expandedHeight = jlimit(
                SignalProbeRail::minimumExpandedHeight,
                maximumHeight,
                probeRailResizeStartHeight + probeRailResizeStartY - event.position.y);
        resized();
        return;
    }
    if (resizingDockSplit) {
        dockSplitRatio = jlimit(0.2f, 0.8f, event.position.x / (float) getWidth());
        requestCanvasRepaint();
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
        notifyOverlayPresentationChanged();
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
    if (draggingSpectralPanNodeId.isNotEmpty()) {
        draggingSpectralPanNodeId = {};
        applyAuthoringResult(authoring.endSpectralPanGesture());
        requestCanvasRepaint();
        return;
    }
    if (draggingVoiceContextSlider.has_value()) {
        if (*draggingVoiceContextSlider != VoiceContextEdit::Control::VoiceLength) {
            applyAuthoringResult(authoring.endVoiceContextSliderGesture());
        }
        draggingVoiceContextSlider.reset();
        requestCanvasRepaint();
        return;
    }
    if (resizingProbeRail) {
        resizingProbeRail = false;
        return;
    }
    if (resizingDockSplit) {
        resizingDockSplit = false;
        settings.getGlobalSetting(AppSettings::GuideDockSplitPercent) = roundToInt(dockSplitRatio * 100.f);
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
    const Rectangle<float> guideShelf = GuideCurveShelf::boundsFor(
            workspace,
            probeRailState,
            dockSplitRatio,
            guideShelfState);
    if (!guideShelfState.minimized && guideShelf.contains(event.position)) {
        const float wheelDelta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY)
                ? wheel.deltaX
                : wheel.deltaY;
        guideShelfState.horizontalOffset = jlimit(
                0.f,
                GuideCurveShelf::maximumHorizontalOffset(
                        workspace,
                        probeRailState,
                        dockSplitRatio,
                        guideShelfState,
                        (int) graph.getGuideCurves().size()),
                guideShelfState.horizontalOffset - wheelDelta * 420.f);
        requestCanvasRepaint();
        return;
    }
    const Rectangle<float> spyWorkspace = GuideCurveShelf::spyWorkspace(
            workspace,
            dockSplitRatio,
            guideShelfState.minimized,
            probeRailState.minimized);
    if (probeRailState.expanded && !probeRailState.minimized
            && SignalProbeRail::boundsFor(spyWorkspace, probeRailState).contains(event.position)) {
        const float wheelDelta = std::abs(wheel.deltaX) > std::abs(wheel.deltaY)
                ? wheel.deltaX
                : wheel.deltaY;
        probeRailState.horizontalOffset = jlimit(
                0.f,
                SignalProbeRail::maximumHorizontalOffset(
                        spyWorkspace,
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
    notifyOverlayPresentationChanged();
    requestCanvasRepaint();
}

void NodeCanvas::mouseMagnify(const MouseEvent& event, float scaleFactor) {
    viewport.zoomAround(event.position, scaleFactor);
    notifyOverlayPresentationChanged();
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
        if (probeDetailState.isOpen()) {
            probeDetailState.close();
            notifyOverlayPresentationChanged();
            requestCanvasRepaint();
            return true;
        }
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
        const bool guideSnapshotUpdated = canvasPresentation.renderOpenGL(
                renderer,
                presentationFrame(),
                (float) openGLContext.getRenderingScale());
        if (guideSnapshotUpdated) {
            Component::SafePointer<NodeCanvas> safeThis(this);
            MessageManager::callAsync([safeThis]() {
                if (safeThis != nullptr) {
                    safeThis->requestCanvasRepaint();
                }
            });
        }
        editorCoordinator.renderOpenGL((float) openGLContext.getRenderingScale());
        if (guideEditor != nullptr && guideEditor->isVisible()) {
            guideEditor->renderOpenGL((float) openGLContext.getRenderingScale());
        }
    } else {
        OpenGLHelpers::clear(kCanvasBackground);
    }
}

void NodeCanvas::openGLContextClosing() {
    editorCoordinator.releaseOpenGLResources();
    if (guideEditorWidget != nullptr) {
        guideEditorWidget->releaseSharedGlResources();
    }

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
        updateHoverAt(mouse);
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
            guideShelfState,
            dockSplitRatio,
            probeRailState,
            probeDetailState,
            globalUnisonPreviewContext
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
    refreshProbeDetail();
}

void NodeCanvas::refreshCompiledStateAsync() {
    compiledStateRefreshPending = false;
    editorCoordinator.clearPreviewCache();
    const NodeGraph& refreshGraph = commands.editingGraph();
    const GraphChangeSet& refreshChange = commands.hasTransientEdit()
            ? commands.transientChanges()
            : document.lastChange();
    presentation.refreshAsync(
            refreshGraph,
            document.revision(),
            refreshChange,
            [safeThis = SafePointer<NodeCanvas>(this)] {
                if (safeThis == nullptr) {
                    return;
                }
                if (!safeThis->commands.hasTransientEdit()) {
                    safeThis->editorCoordinator.updateHost(
                            safeThis->commands.editingGraph().findNode(safeThis->expandedNodeId),
                            safeThis->canvasContentBounds());
                }
                safeThis->openGLContext.triggerRepaint();
                safeThis->refreshProbeDetail();
                safeThis->requestCanvasRepaint();
            });
}

void NodeCanvas::setPreviewVoiceLength(double seconds) {
    const double duration = jlimit(
            CycleDsp::voiceLengthSeconds(0.f),
            CycleDsp::voiceLengthSeconds(1.f),
            seconds);
    if (std::abs(globalUnisonPreviewContext.voiceDurationSeconds - duration) < 0.0005) {
        return;
    }
    globalUnisonPreviewContext.voiceDurationSeconds = duration;
    settings.getGlobalSetting(AppSettings::PreviewVoiceLengthMilliseconds) =
            roundToInt(duration * 1000.0);
    editStatusMessage = "Voice length: " + String(duration, 2) + " seconds";
    requestCanvasRepaint();
}

void NodeCanvas::openProbeDetail(const String& probeId) {
    const int midiNote = GraphPresentationModel::auditionMidiNoteForProbe(
            commands.editingGraph(),
            probeId);
    const size_t resolution = SignalProbeDetailView::resolutionForMidiNote(
            midiNote);
    auto preview = presentation.captureProbePreview(
            commands.editingGraph(),
            probeId,
            resolution,
            midiNote);
    if (!preview.has_value()) {
        probeDetailState.close();
        return;
    }

    editorCoordinator.close();
    probeDetailState.open(
            std::move(*preview),
            SignalProbeRail::renderSemanticForProbe(
                    commands.editingGraph(), probeId).scalePolicy,
            SignalProbeRail::ordinalForProbe(graph, probeId),
            midiNote,
            resolution);
    notifyOverlayPresentationChanged();
}

void NodeCanvas::refreshProbeDetail() {
    if (!probeDetailState.isOpen()) {
        return;
    }

    const String probeId = probeDetailState.probeId;
    openProbeDetail(probeId);
}

UnisonPreviewContext NodeCanvas::unisonPreviewContext() const {
    return NodeCanvasPresentation::unisonPreviewContextFor(
            presentation.compileResult().plan,
            expandedNodeId,
            globalUnisonPreviewContext);
}

bool NodeCanvas::applyAuthoringResult(const NodeCanvasAuthoringResult& result) {
    if (!result.handled) {
        return false;
    }

    if (result.graphChanged) {
        compiledStateRefreshPending = false;
        editorCoordinator.clearPreviewCache();

        if (document.lastChange().probesChanged) {
            probeRailState.horizontalOffset = jmin(
                    probeRailState.horizontalOffset,
                    SignalProbeRail::maximumHorizontalOffset(
                            GuideCurveShelf::spyWorkspace(
                                    getLocalBounds().toFloat(),
                                    dockSplitRatio,
                                    guideShelfState.minimized,
                                    probeRailState.minimized),
                            (int) graph.getSignalProbes().size()));
            if (SignalProbeRail::ordinalForProbe(graph, probeDetailState.probeId) == 0) {
                probeDetailState.close();
            }
            resized();
        }
    }
    if (result.effects.resetInteraction) {
        interaction.reset();
        spliceTargetEdgeIndex = -1;
    }
    if (result.effects.editorBindingChanged) {
        editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
        notifyOverlayPresentationChanged();
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
            selectedEdgeIndex,
            globalUnisonPreviewContext.voiceDurationSeconds,
            probeRailState.refreshMode,
            SignalProbeRail::refreshModeBoundsFor(
                    GuideCurveShelf::spyWorkspace(
                            getLocalBounds().toFloat(),
                            dockSplitRatio,
                            guideShelfState.minimized,
                            probeRailState.minimized),
                    probeRailState),
            probeDetailState.probeId,
            probeDetailState.resolution,
            probeDetailState.renderResult.gridColumns,
            probeDetailState.renderResult.gridRows,
            probeDetailState.isOpen()
                    ? SignalProbeDetailView::boundsFor(canvasContentBounds())
                    : Rectangle<float> {},
            canvasContentBounds()
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

    refreshCompiledStateAsync();
}

var NodeCanvas::exportAutomationState() const {
    return automation.exportState(automationPresentationState());
}

String NodeCanvas::exportGraphJson() const {
    return automation.exportGraphJson();
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

bool NodeCanvas::copyAudioPlan(
        GraphExecutionPlan& plan,
        uint64_t& revision) const {
    if (!presentation.compileResult().succeeded()) {
        return false;
    }
    plan = presentation.compileResult().plan;
    revision = presentation.revision();
    return true;
}

Rectangle<float> NodeCanvas::visibleWorldBoundsForOverlay() const {
    return viewport.toWorld(canvasContentBounds());
}

Rectangle<int> NodeCanvas::boundsForWorldOverlay(Rectangle<float> worldBounds) const {
    return viewport.toScreen(worldBounds).toNearestInt();
}

Point<float> NodeCanvas::worldPositionForOverlay(Point<float> canvasPosition) const {
    return viewport.toWorld(canvasPosition);
}

Rectangle<float> NodeCanvas::expandedEditorBoundsForOverlay() const {
    if (probeDetailState.isOpen()) {
        return SignalProbeDetailView::boundsFor(canvasContentBounds());
    }

    const Node* expandedNode = queries.findNode(expandedNodeId);
    return expandedNode != nullptr
            ? editorCoordinator.boundsFor(expandedNode, canvasContentBounds())
            : Rectangle<float> {};
}

void NodeCanvas::setOverlayPresentationChangedCallback(std::function<void()> callback) {
    overlayPresentationChanged = std::move(callback);
}

void NodeCanvas::notifyOverlayPresentationChanged() {
    if (overlayPresentationChanged) {
        overlayPresentationChanged();
    }
}

std::optional<Rectangle<float>> NodeCanvas::performanceKeyboardBounds() const {
    return commands.editingGraph().getPerformanceKeyboardBounds();
}

void NodeCanvas::beginPerformanceKeyboardMove() {
    commands.beginCompoundEdit();
}

void NodeCanvas::movePerformanceKeyboard(Rectangle<float> worldBounds) {
    commands.setPerformanceKeyboardBounds(worldBounds);
}

void NodeCanvas::endPerformanceKeyboardMove() {
    commands.commitCompoundEdit();
}

void NodeCanvas::storePerformanceKeyboardBounds(Rectangle<float> worldBounds) {
    commands.setPerformanceKeyboardBounds(worldBounds);
}

File NodeCanvas::snapshotFile() const {
    return File::getSpecialLocation(File::userApplicationDataDirectory)
            .getChildFile("CycleV2")
            .getChildFile("graph-snapshot.cyclegraph");
}

bool NodeCanvas::saveGraphToFile(const File& file) {
    return applyAuthoringResult(authoring.saveGraph(file));
}

bool NodeCanvas::loadGraphFromFile(const File& file) {
    const bool loaded = applyAuthoringResult(authoring.loadGraph(file));
    if (loaded) {
        probeDetailState.close();
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
        notifyOverlayPresentationChanged();
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
    notifyOverlayPresentationChanged();
}

void NodeCanvas::openGuideEditor(const String& guideId) {
    const GuideCurveResource* guide = graph.findGuideCurve(guideId);
    if (guide == nullptr) {
        return;
    }

    if (guideEditor == nullptr) {
        guideEditorWidget = std::make_unique<Effect2DWidget>(true);
        guideEditor = std::make_unique<GuideCurveEditorComponent>(*guideEditorWidget);
        guideEditor->setDelegate(this);
        guideEditor->setTitle("Guide Curve");
        addAndMakeVisible(*guideEditor);
    }

    expandedNodeId = {};
    editorCoordinator.close();
    expandedGuideId = guideId;
    guideEditor->setGuideResource(*guide);
    guideEditor->setBounds(canvasContentBounds().reduced(36.f, 24.f).toNearestInt());
    guideEditor->setVisible(true);
    guideEditor->toFront(false);
    notifyOverlayPresentationChanged();
}

void NodeCanvas::closeGuideEditor() {
    if (guideTransactionBaseRevision.has_value()) {
        commands.cancelTransientEdit();
        guideTransactionBaseRevision.reset();
        refreshCompiledStateAsync();
    }
    expandedGuideId = {};
    if (guideEditor != nullptr) {
        guideEditor->setVisible(false);
    }
    notifyOverlayPresentationChanged();
    requestCanvasRepaint();
}

void NodeCanvas::closeEffect2DEditor() {
    closeGuideEditor();
}

void NodeCanvas::repaintEffect2DEditorOpenGL() {
    openGLContext.triggerRepaint();
    if (guideEditor != nullptr) {
        guideEditor->repaint();
    }
}

bool NodeCanvas::publishEffect2DState(
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls) {
    if (expandedGuideId.isEmpty()) {
        return false;
    }
    const GuideCurveResource* durableGuide = document.graph().findGuideCurve(expandedGuideId);
    if (durableGuide == nullptr) {
        return false;
    }
    const uint64_t durableBaseRevision = guideTransactionBaseRevision.value_or(
            durableGuide->model != nullptr ? durableGuide->model->revision() : 0);
    const auto result = commands.publishGuideCurveState({
            expandedGuideId,
            durableBaseRevision,
            std::move(model),
            controls
    });
    if (!result.succeeded()) {
        return false;
    }
    if (probeRailState.refreshMode == ProbeRefreshMode::LiveLatest) {
        presentation.refresh(
                commands.editingGraph(),
                document.revision(),
                commands.transientChanges());
    }
    requestCanvasRepaint();
    return true;
}

void NodeCanvas::beginEffect2DTransaction() {
    const GuideCurveResource* guide = document.graph().findGuideCurve(expandedGuideId);
    if (guide == nullptr || guideTransactionBaseRevision.has_value()) {
        return;
    }
    guideTransactionBaseRevision = guide->model != nullptr ? guide->model->revision() : 0;
    commands.beginTransientEdit();
}

void NodeCanvas::commitEffect2DTransaction() {
    if (!guideTransactionBaseRevision.has_value()) {
        return;
    }
    commands.commitTransientEdit();
    guideTransactionBaseRevision.reset();
    refreshCompiledStateAsync();
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
    refreshCompiledStateAsync();
}

Point<float> NodeCanvas::nodeEditorCreationPosition() const {
    return viewportCentreWorld();
}

void NodeCanvas::rebindNodeEditor() {
    editorCoordinator.updateHost(queries.findNode(expandedNodeId), canvasContentBounds());
}

void NodeCanvas::rebindNodeEditorTransient() {
    const Node* node = commands.editingGraph().findNode(expandedNodeId);
    if (node != nullptr) {
        editorCoordinator.host().rebindTransient(*node);
    }
}

void NodeCanvas::recordNodeEditorMovement(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint) {
    const bool deferred = probeRailState.refreshMode == ProbeRefreshMode::OnGestureCommit;
    presentation.recordEditorMovement(nodeId, field, effectiveFingerprint, deferred);
    if (!deferred) {
        scheduleCompiledStateRefresh();
    }
}

void NodeCanvas::commitNodeEditorLocalState(
        const String& nodeId,
        const String& field,
        uint64_t effectiveFingerprint,
        uint64_t documentRevision) {
    presentation.commitLocalEditorState(
            nodeId,
            field,
            effectiveFingerprint,
            documentRevision);
}

Effect2DWidget* NodeCanvas::effect2DWidget(const Node& node) {
    return &editorCoordinator.previewResources().effect2DWidget(node);
}

TrimeshWidget* NodeCanvas::trimeshWidget(const Node& node) {
    return &editorCoordinator.previewResources().trimeshWidget(node);
}

TrimeshWidget* NodeCanvas::findTrimeshWidget(const String& nodeId) {
    return editorCoordinator.previewResources().findTrimeshWidget(nodeId);
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
            true,
            globalUnisonPreviewContext
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
