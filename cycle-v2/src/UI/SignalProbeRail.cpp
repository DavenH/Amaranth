#include <algorithm>
#include <limits>
#include <unordered_map>

#include "UI/SignalProbeRail.h"

#include "Graph/GraphValidator.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

namespace {

void paintProbeOrdinal(Graphics& graphics, Rectangle<float> previewBounds, int ordinal) {
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.86f));
    graphics.setFont(FontOptions(12.f));
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

}

Rectangle<float> SignalProbeRail::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return WorkspaceDock::layout(
            workspace,
            { state.expanded, false, false, state.expandedHeight, 0.5f }).dock;
}

Rectangle<float> SignalProbeRail::contentBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return WorkspaceDock::layout(
            workspace,
            { state.expanded, false, false, state.expandedHeight, 0.5f }).content;
}

Rectangle<float> SignalProbeRail::resizeHandleFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return WorkspaceDock::layout(
            workspace,
            { state.expanded, false, false, state.expandedHeight, 0.5f }).resizeHandle;
}

Rectangle<float> SignalProbeRail::collapseHandleFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return WorkspaceDock::layout(
            workspace,
            { state.expanded, false, false, state.expandedHeight, 0.5f }).collapseHandle;
}

Rectangle<float> SignalProbeRail::refreshModeBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    if (!state.expanded) {
        return {};
    }
    const Rectangle<float> rail = boundsFor(workspace, state);
    Rectangle<float> header = WorkspaceDock::headerBounds(rail);
    header.removeFromRight(WorkspaceDock::controlSize + 8.f);
    return header.removeFromRight(104.f);
}

Rectangle<float> SignalProbeRail::minimizeButtonBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    if (!state.expanded || state.minimized) {
        return {};
    }
    Rectangle<float> header = WorkspaceDock::headerBounds(boundsFor(workspace, state));
    return header.removeFromRight(WorkspaceDock::controlSize);
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

float SignalProbeRail::maximumHorizontalOffset(
        Rectangle<float> workspace,
        int probeCount) {
    const float gaps = (float) jmax(0, probeCount - 1) * WorkspaceDock::tileGap;
    const float contentWidth = WorkspaceDock::shelfPadding * 2.f
            + (float) probeCount * WorkspaceDock::tileWidth + gaps;
    return jmax(0.f, contentWidth - workspace.getWidth());
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
        const Edge* edge = graphEdgeFor(graph, sceneEdge.edgeIndex);
        if (edge == nullptr || edge->sourceNodeId != probe.sourceNodeId
                || edge->sourcePortId != probe.sourcePortId) {
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
    const Edge* edge = anchor == nullptr ? nullptr : graphEdgeFor(graph, anchor->edgeIndex);
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
        return anchor->cablePath.getPointAlongPath(
                anchor->cablePath.getLength() * jlimit(0.f, 1.f, probe.tapPosition));
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
        const SignalProbeRailState& state) const {
    const auto probes = orderedProbes(graph);
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Point<float> marker = markerCentre(probe, graph, scene);
        if (marker == Point<float>()) {
            continue;
        }

        const Colour colour = colourForProbe(probe, graph, scene);
        const bool active = probe.id == state.hoveredProbeId || probe.id == state.selectedProbeId;
        if (probe.id == state.hoveredProbeId && state.expanded) {
            const Point<float> tileTarget = tileBoundsFor(workspace, state, index).getCentre().withY(
                    tileBoundsFor(workspace, state, index).getY());
            Path tether;
            tether.startNewSubPath(marker);
            tether.cubicTo(
                    marker.translated(0.f, 42.f),
                    tileTarget.translated(0.f, -42.f),
                    tileTarget);
            graphics.setColour(colour.withAlpha(0.32f));
            graphics.strokePath(tether, PathStrokeType(2.f, PathStrokeType::curved));
        }

        const Rectangle<float> badge(16.f, 16.f);
        graphics.setColour(CanvasChromePalette::canvasBackground);
        graphics.fillEllipse(badge.withCentre(marker));
        graphics.setColour(colour);
        graphics.drawEllipse(badge.withCentre(marker), active ? 2.5f : 1.8f);
        graphics.setFont(FontOptions(9.f));
        graphics.drawText(String(index + 1), badge.withCentre(marker), Justification::centred);
    }
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

void SignalProbeRail::paintRail(
        Graphics& graphics,
        const NodeGraph& graph,
        const GraphPreviewResult& previews,
        Rectangle<float> workspace,
        const SignalProbeRailState& state,
        const WorkspaceDockFocus& focus) {
    const Rectangle<float> rail = boundsFor(workspace, state);
    graphics.setColour(CanvasChromePalette::dockSurface.withAlpha(0.96f));
    graphics.fillRect(rail);
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawHorizontalLine(roundToInt(rail.getY()), rail.getX(), rail.getRight());

    const auto probes = orderedProbes(graph);
    if (state.minimized) {
        Rectangle<float> labelArea = rail;
        Rectangle<float> drawerButton = labelArea.removeFromTop(WorkspaceDock::drawerWidth).reduced(4.f);
        WorkspaceDock::paintIconButton(
                graphics,
                drawerButton,
                WorkspaceDockIcon::ChevronLeft,
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
    if (!state.expanded) {
        return;
    }

    const Rectangle<float> minimize = minimizeButtonBoundsFor(workspace, state);
    WorkspaceDock::paintIconButton(
            graphics,
            minimize,
            WorkspaceDockIcon::ChevronRight,
            focus.target == WorkspaceDockFocusTarget::SpyMinimize);

    const Rectangle<float> header = WorkspaceDock::headerBounds(rail);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(12.f));
    graphics.drawText(
            "Spies",
            header.withTrimmedLeft(22.f).withWidth(110.f),
            Justification::centredLeft);

    const Rectangle<float> refreshMode = refreshModeBoundsFor(workspace, state);
    const bool refreshFocused = focus.target == WorkspaceDockFocusTarget::SpyRefresh;
    const auto refreshColours = CanvasChromePalette::control(refreshFocused
            ? CanvasChromeControlState::Focused
            : CanvasChromeControlState::Resting);
    graphics.setColour(refreshColours.surface);
    graphics.fillRoundedRectangle(refreshMode, CanvasChromeMetrics::controlCornerRadius);
    graphics.setColour(refreshColours.border);
    graphics.drawRoundedRectangle(
            refreshMode,
            CanvasChromeMetrics::controlCornerRadius,
            refreshFocused ? 2.f : 1.f);
    graphics.setColour(refreshColours.text);
    graphics.setFont(FontOptions(12.f));
    graphics.drawText(
            state.refreshMode == ProbeRefreshMode::LiveLatest ? "Live" : "On Release",
            refreshMode,
            Justification::centred);

    if (probes.empty()) {
        const Rectangle<float> vacancy = WorkspaceDock::vacancyBounds(rail);
        graphics.setColour(CanvasChromePalette::insetBackground);
        graphics.fillRoundedRectangle(vacancy, CanvasChromeMetrics::tileCornerRadius);
        graphics.setColour(CanvasChromePalette::border.withAlpha(0.75f));
        graphics.drawRoundedRectangle(
                vacancy,
                CanvasChromeMetrics::tileCornerRadius,
                1.f);
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(FontOptions(12.f));
        graphics.drawText("No spies", vacancy.reduced(14.f), Justification::centredLeft);
        return;
    }

    Graphics::ScopedSaveState tileClip(graphics);
    graphics.reduceClipRegion(rail.toNearestInt());
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Rectangle<float> tile = tileBoundsFor(workspace, state, index);
        const auto* preview = previewFor(previews, probe.id);
        const Colour colour = preview != nullptr && preview->connected
                ? colourForDomain(preview->domain)
                : CanvasChromePalette::mutedText;
        const bool selected = probe.id == state.selectedProbeId;
        const bool hovered = probe.id == state.hoveredProbeId;
        const bool focused = focus.target == WorkspaceDockFocusTarget::SpyTile
                && focus.itemId == probe.id;
        WorkspaceDock::paintTileChrome(
                graphics,
                tile,
                colour,
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

        const PreviewModuleRole displayRole = preview->sourceRole
                == PreviewModuleRole::MeshSurface
                ? PreviewModuleRole::MeshSurface
                : PreviewModuleRole::SignalSpy;
        NodePreviewResult compactResult {
                "probe-preview-" + probe.id,
                displayRole,
                preview->values,
                {},
                preview->gridColumns,
                preview->gridRows,
                preview->domain,
                preview->frequencySampling,
                preview->frequencyMidiNote
        };
        Node displayNode;
        displayNode.id = "probe-preview-" + probe.id;
        displayNode.kind = NodeKind::GenericProcessor;
        NodeRenderSemantic semantic = renderSemanticForProbe(graph, probe.id);
        if (semantic.domain == PortDomain::ControlSignal) {
            semantic.domain = preview->domain;
        }
        renderer.paint(graphics, {
                displayNode,
                &compactResult,
                previewBounds,
                TrimeshRenderProfile::fromSemantic(semantic),
                1.f,
                true
        });
        paintProbeOrdinal(graphics, previewBounds, index + 1);
    }
    WorkspaceDock::paintOverflowFeedback(
            graphics,
            rail,
            state.horizontalOffset,
            maximumHorizontalOffset(workspace, (int) probes.size()));
}

}
