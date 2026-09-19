#include <algorithm>
#include <limits>
#include <unordered_map>

#include "UI/SignalProbeRail.h"

#include "Graph/GraphValidator.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/NodeCableRenderer.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

namespace {

constexpr float kCableAnnotationBaseDiameter = 23.04f;

float probeRowWidth(int probeCount) {
    return WorkspaceDock::shelfPadding * 2.f
            + (float) probeCount * WorkspaceDock::tileWidth
            + (float) jmax(0, probeCount - 1) * WorkspaceDock::tileGap;
}

void paintProbeOrdinal(Graphics& graphics, Rectangle<float> previewBounds, int ordinal) {
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.86f));
    graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
    graphics.drawText(
            String(ordinal),
            previewBounds.reduced(7.f).removeFromTop(20.f),
            Justification::centredLeft);
}

const Edge* graphEdgeFor(const NodeGraph& graph, int edgeIndex) {
    return isPositiveAndBelow(edgeIndex, (int) graph.getEdges().size())
            ? &graph.getEdges()[(size_t) edgeIndex]
            : nullptr;
}

const Edge* graphEdgeForProbe(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeSceneEdge& sceneEdge) {
    for (const int edgeIndex : sceneEdge.edgeIndices) {
        const Edge* edge = graphEdgeFor(graph, edgeIndex);
        if (edge != nullptr
                && edge->sourceNodeId == probe.sourceNodeId
                && edge->sourcePortId == probe.sourcePortId) {
            return edge;
        }
    }
    return nullptr;
}

}

Rectangle<float> SignalProbeRail::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return WorkspaceDock::spyRowBounds(workspace, state.expanded, state.expandedHeight);
}

Rectangle<float> SignalProbeRail::minimizeButtonBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    if (!state.expanded || state.minimized) {
        return {};
    }
    return WorkspaceDock::spyControls(boundsFor(workspace, state)).minimize;
}

Rectangle<float> SignalProbeRail::tileBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state,
        int tileIndex) {
    return WorkspaceDock::tileBounds(
            boundsFor(workspace, state),
            tileIndex,
            state.horizontalOffset);
}

Rectangle<float> SignalProbeRail::scrollAreaFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state,
        int probeCount) {
    if (!state.expanded || state.minimized || probeCount < 1) {
        return {};
    }
    const Rectangle<float> rail = boundsFor(workspace, state);
    return rail.withTrimmedTop(WorkspaceDock::headerHeight)
            .withWidth(jmin(rail.getWidth(), probeRowWidth(probeCount)));
}

float SignalProbeRail::maximumHorizontalOffset(
        Rectangle<float> workspace,
        int probeCount) {
    return jmax(0.f, probeRowWidth(probeCount) - workspace.getWidth());
}

int SignalProbeRail::ordinalForProbe(const NodeGraph& graph, const String& probeId) {
    const auto probes = orderedProbes(graph);
    const auto found = std::find_if(probes.begin(), probes.end(), [&](const auto* probe) {
        return probe->id == probeId;
    });
    return found == probes.end()
            ? 0
            : (int) std::distance(probes.begin(), found) + 1;
}

std::vector<String> SignalProbeRail::orderedProbeIds(const NodeGraph& graph) {
    std::vector<String> ids;
    for (const auto* probe : orderedProbes(graph)) {
        ids.push_back(probe->id);
    }
    return ids;
}

NodeRenderSemantic SignalProbeRail::renderSemanticForProbe(
        const NodeGraph& graph,
        const String& probeId) {
    const auto found = std::find_if(
            graph.getSignalProbes().begin(),
            graph.getSignalProbes().end(),
            [&](const auto& probe) {
                return probe.id == probeId;
            });
    if (found == graph.getSignalProbes().end()) {
        return {};
    }

    return GraphRenderSemanticResolver().semanticForNodeOutput(
            graph,
            found->sourceNodeId,
            found->sourcePortId);
}

std::vector<const SignalProbe*> SignalProbeRail::orderedProbes(const NodeGraph& graph) {
    std::vector<const SignalProbe*> probes;
    probes.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        probes.push_back(&probe);
    }

    const auto& nodes = graph.getNodes();
    std::unordered_map<std::string, size_t> nodeIndices;
    nodeIndices.reserve(nodes.size());
    for (size_t index = 0; index < nodes.size(); ++index) {
        nodeIndices.emplace(nodes[index].id.toStdString(), index);
    }

    std::vector<std::vector<size_t>> destinations(nodes.size());
    std::vector<int> incoming(nodes.size());
    for (const auto& edge : graph.getEdges()) {
        const auto source = nodeIndices.find(edge.sourceNodeId.toStdString());
        const auto destination = nodeIndices.find(edge.destNodeId.toStdString());
        if (source == nodeIndices.end() || destination == nodeIndices.end()) {
            continue;
        }

        destinations[source->second].push_back(destination->second);
        ++incoming[destination->second];
    }

    std::vector<int> depths(nodes.size());
    std::vector<size_t> pending;
    pending.reserve(nodes.size());
    for (size_t index = 0; index < incoming.size(); ++index) {
        if (incoming[index] == 0) {
            pending.push_back(index);
        }
    }

    for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const size_t source = pending[cursor];
        for (const size_t destination : destinations[source]) {
            depths[destination] = jmax(depths[destination], depths[source] + 1);
            if (--incoming[destination] == 0) {
                pending.push_back(destination);
            }
        }
    }

    const auto nodeIndex = [&](const String& nodeId) {
        const auto found = nodeIndices.find(nodeId.toStdString());
        return found == nodeIndices.end() ? nodes.size() : found->second;
    };

    std::stable_sort(probes.begin(), probes.end(), [&](const auto* left, const auto* right) {
        const size_t leftIndex = nodeIndex(left->sourceNodeId);
        const size_t rightIndex = nodeIndex(right->sourceNodeId);
        const int leftDepth = leftIndex < depths.size() ? depths[leftIndex] : std::numeric_limits<int>::max();
        const int rightDepth = rightIndex < depths.size() ? depths[rightIndex] : std::numeric_limits<int>::max();
        if (leftDepth != rightDepth) {
            return leftDepth < rightDepth;
        }
        if (left->railOrder != right->railOrder) {
            return left->railOrder < right->railOrder;
        }
        return leftIndex < rightIndex;
    });
    return probes;
}

const NodeSceneEdge* SignalProbeRail::anchorFor(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene) {
    const NodeSceneEdge* fallback = nullptr;
    for (const auto& sceneEdge : scene.edges) {
        const Edge* edge = graphEdgeForProbe(probe, graph, sceneEdge);
        if (edge == nullptr) {
            continue;
        }
        if (fallback == nullptr) {
            fallback = &sceneEdge;
        }
        if (edge->destNodeId == probe.anchorDestNodeId
                && edge->destPortId == probe.anchorDestPortId) {
            return &sceneEdge;
        }
    }
    return fallback;
}

Colour SignalProbeRail::colourForProbe(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene) {
    const NodeSceneEdge* anchor = SignalProbeRail::anchorFor(probe, graph, scene);
    const Edge* edge = anchor == nullptr ? nullptr : graphEdgeForProbe(probe, graph, *anchor);
    if (edge == nullptr) {
        return CanvasChromePalette::mutedText;
    }

    const PortDomain domain = edge->isAttachment()
            ? edge->domain
            : GraphValidator().resolvedDomainForEdge(graph, *edge);
    return colourForDomain(domain);
}

Point<float> SignalProbeRail::markerCentre(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene) {
    const NodeSceneEdge* anchor = anchorFor(probe, graph, scene);
    if (anchor != nullptr) {
        float position = 0.5f;
        if (anchor->inlinePan) {
            const Edge* edge = graphEdgeForProbe(probe, graph, *anchor);
            position = edge == graphEdgeFor(graph, anchor->edgeIndices.front())
                    ? 1.f / 3.f
                    : 2.f / 3.f;
        }
        return anchor->cablePath.getPointAlongPath(
                anchor->cablePath.getLength() * position);
    }

    const String sourceSemanticId = "output:" + probe.sourceNodeId + "." + probe.sourcePortId;
    const auto source = std::find_if(scene.targets.begin(), scene.targets.end(), [&](const auto& target) {
        return target.semanticId == sourceSemanticId;
    });
    return source != scene.targets.end() ? source->bounds.getCentre() : Point<float>();
}

String SignalProbeRail::probeAt(
        Point<float> position,
        Rectangle<float> workspace,
        const NodeGraph& graph,
        const SignalProbeRailState& state) const {
    if (!state.expanded || state.minimized) {
        return {};
    }
    const Rectangle<float> visibleTiles = boundsFor(workspace, state)
            .withTrimmedTop(WorkspaceDock::headerHeight);
    if (!visibleTiles.contains(position)) {
        return {};
    }
    const auto probes = orderedProbes(graph);
    for (int index = 0; index < (int) probes.size(); ++index) {
        if (tileBoundsFor(workspace, state, index).contains(position)) {
            return probes[(size_t) index]->id;
        }
    }
    return {};
}

String SignalProbeRail::markerProbeAt(
        Point<float> position,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene) const {
    for (const auto& probe : graph.getSignalProbes()) {
        if (probe.sourceNodeId.isEmpty()) {
            continue;
        }
        const Point<float> centre = markerCentre(probe, graph, scene);
        if (Rectangle<float>(18.f, 18.f).withCentre(centre).contains(position)) {
            return probe.id;
        }
    }
    return {};
}

void SignalProbeRail::paintCableAnnotations(
        Graphics& graphics,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        Rectangle<float> workspace,
        const SignalProbeRailState& state,
        float zoom) const {
    const auto probes = orderedProbes(graph);
    const float railTop = boundsFor(workspace, state).getY();
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Point<float> marker = markerCentre(probe, graph, scene);
        if (marker == Point<float>()) {
            continue;
        }

        const Colour colour = colourForProbe(probe, graph, scene);
        const bool active = probe.id == state.hoveredProbeId || probe.id == state.selectedProbeId;
        if (probe.id == state.hoveredProbeId && state.expanded) {
            const Point<float> tileTarget {
                    tileBoundsFor(workspace, state, index).getCentreX(),
                    railTop
            };
            Path tether;
            tether.startNewSubPath(marker);
            tether.cubicTo(
                    marker.translated(0.f, 42.f),
                    tileTarget.translated(0.f, -42.f),
                    tileTarget);
            graphics.setColour(colour.withAlpha(0.32f));
            graphics.strokePath(tether, PathStrokeType(2.f, PathStrokeType::curved));
        }

        const float diameter = cableAnnotationDiameter(zoom);
        const Rectangle<float> badge(diameter, diameter);
        graphics.setColour(CanvasChromePalette::canvasBackground);
        graphics.fillEllipse(badge.withCentre(marker));
        graphics.setColour(colour);
        const float scale = diameter / 16.f;
        graphics.drawEllipse(badge.withCentre(marker), (active ? 2.5f : 1.8f) * scale);
        graphics.setFont(FontOptions(9.f * scale));
        graphics.drawText(String(index + 1), badge.withCentre(marker), Justification::centred);
    }
}

float SignalProbeRail::cableAnnotationDiameter(float zoom) {
    return kCableAnnotationBaseDiameter * NodeCableRenderer::scaleForZoom(zoom);
}

const GraphPreviewResult::SignalProbePreview* SignalProbeRail::previewFor(
        const GraphPreviewResult& previews,
        const String& probeId) const {
    for (const auto& preview : previews.probes) {
        if (preview.probeId == probeId) {
            return &preview;
        }
    }
    return nullptr;
}

void SignalProbeRail::paintCachedPreview(
        Graphics& graphics,
        const NodeGraph& graph,
        const SignalProbe& probe,
        const GraphPreviewResult::SignalProbePreview& preview,
        Rectangle<float> previewBounds,
        float physicalScale) {
    NodeRenderSemantic semantic = renderSemanticForProbe(graph, probe.id);
    if (semantic.domain == PortDomain::ControlSignal) {
        semantic.domain = preview.domain;
    }

    const Rectangle<int> logicalBounds = previewBounds.getSmallestIntegerContainer();
    const SignalProbePreviewTileCacheAccess cache = previewTileCache.access(
            preview,
            semantic,
            logicalBounds,
            physicalScale);
    if (!cache.hit) {
        NodePreviewResult compactResult {
                "probe-preview-" + probe.id,
                PreviewModuleRole::SignalSpy,
                preview.values,
                {},
                preview.gridColumns,
                preview.gridRows,
                preview.domain,
                preview.frequencySampling,
                preview.frequencyMidiNote
        };
        Node displayNode;
        displayNode.id = "probe-preview-" + probe.id;
        displayNode.kind = NodeKind::GenericProcessor;
        Graphics imageGraphics(*cache.image);
        imageGraphics.addTransform(AffineTransform(
                physicalScale,
                0.f,
                -logicalBounds.getX() * physicalScale,
                0.f,
                physicalScale,
                -logicalBounds.getY() * physicalScale));
        renderer.paint(imageGraphics, {
                displayNode,
                &compactResult,
                previewBounds,
                TrimeshRenderProfile::fromSemantic(semantic),
                1.f,
                true
        });
    }
    previewTileCache.draw(graphics, cache);
}

void SignalProbeRail::paintRail(
        Graphics& graphics,
        const NodeGraph& graph,
        const GraphPreviewResult& previews,
        Rectangle<float> workspace,
    const SignalProbeRailState& state,
    const WorkspaceDockFocus& focus) {
    const Rectangle<float> rail = boundsFor(workspace, state);
    const auto probes = orderedProbes(graph);
    if (!state.expanded || probes.empty()) {
        return;
    }
    if (state.minimized) {
        graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.92f));
        graphics.fillRoundedRectangle(rail, CanvasChromeMetrics::panelCornerRadius);
        Rectangle<float> labelArea = rail;
        Rectangle<float> drawerButton = labelArea.removeFromTop(WorkspaceDock::drawerWidth).reduced(4.f);
        WorkspaceDock::paintIconButton(
                graphics,
                drawerButton,
                WorkspaceDockIcon::ChevronRight,
                focus.target == WorkspaceDockFocusTarget::SpyDrawer);
        Graphics::ScopedSaveState labelTransform(graphics);
        graphics.addTransform(AffineTransform::rotation(
                MathConstants<float>::halfPi,
                labelArea.getCentreX(),
                labelArea.getCentreY()));
        graphics.drawText(
                "SPIES",
                Rectangle<float>(labelArea.getHeight() - 8.f, labelArea.getWidth())
                        .withCentre(labelArea.getCentre()),
                Justification::centred);
        return;
    }
    const Rectangle<float> minimize = minimizeButtonBoundsFor(workspace, state);
    WorkspaceDock::paintIconButton(
            graphics,
            minimize,
            WorkspaceDockIcon::ChevronLeft,
            focus.target == WorkspaceDockFocusTarget::SpyMinimize);

    const Rectangle<float> label = WorkspaceDock::spyControls(rail).label;
    graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.94f));
    graphics.fillRoundedRectangle(label, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
    graphics.drawText(
            "Spies",
            label,
            Justification::centred);

    Graphics::ScopedSaveState tileClip(graphics);
    graphics.reduceClipRegion(rail.toNearestInt());
    uint64_t previewElapsed {};
    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    previewTileCache.beginFrame();
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Rectangle<float> tile = tileBoundsFor(workspace, state, index);
        const auto* preview = previewFor(previews, probe.id);
        const bool selected = probe.id == state.selectedProbeId;
        const bool hovered = probe.id == state.hoveredProbeId;
        const bool focused = focus.target == WorkspaceDockFocusTarget::SpyTile
                && focus.itemId == probe.id;
        WorkspaceDock::paintTileChrome(
                graphics,
                tile,
                selected,
                hovered,
                focused);

        const Rectangle<float> previewBounds = tile.reduced(7.f);
        if (preview == nullptr || !preview->connected) {
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.drawText("Disconnected", previewBounds, Justification::centred);
            paintProbeOrdinal(graphics, previewBounds, index + 1);
            continue;
        }

        const uint64_t previewStartedAt = performanceObserver != nullptr
                ? performanceObserver->presentationTimestamp()
                : 0;
        paintCachedPreview(
                graphics,
                graph,
                probe,
                *preview,
                previewBounds,
                physicalScale);
        if (performanceObserver != nullptr) {
            previewElapsed += performanceObserver->presentationTimestamp() - previewStartedAt;
        }
        paintProbeOrdinal(graphics, previewBounds, index + 1);
    }
    const SignalProbePreviewTileCacheStats stats = previewTileCache.endFrame();
    if (performanceObserver != nullptr) {
        performanceObserver->presentationStageCompleted(
                NodeCanvasPresentationStage::SpyRailPreviews,
                previewElapsed);
        performanceObserver->spyPreviewTileCacheCompleted(
                stats.hits,
                stats.misses,
                previewElapsed);
    }
    WorkspaceDock::paintOverflowFeedback(
            graphics,
            rail,
            state.horizontalOffset,
            maximumHorizontalOffset(workspace, (int) probes.size()));
}

}
