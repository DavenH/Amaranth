#include "SignalProbeRail.h"

#include "../Graph/GraphValidator.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace CycleV2 {

namespace {

const Colour kRailBackground { 0xf51a212a };
const Colour kRailBorder { 0xff445261 };
const Colour kTileBackground { 0xff11171d };
const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kTileWidth = 184.f;
constexpr float kTileGap = 10.f;
constexpr float kRailPadding = 12.f;

const Edge* graphEdgeFor(const NodeGraph& graph, int edgeIndex) {
    return isPositiveAndBelow(edgeIndex, (int) graph.getEdges().size())
            ? &graph.getEdges()[(size_t) edgeIndex]
            : nullptr;
}

}

Rectangle<float> SignalProbeRail::boundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    const float maximumHeight = jmax(minimumExpandedHeight, workspace.getHeight() * 0.4f);
    const float height = state.expanded
            ? jlimit(minimumExpandedHeight, maximumHeight, state.expandedHeight)
            : collapsedHeight;
    return workspace.removeFromBottom(height);
}

Rectangle<float> SignalProbeRail::contentBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return workspace.withTrimmedBottom(boundsFor(workspace, state).getHeight());
}

Rectangle<float> SignalProbeRail::resizeHandleFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    return state.expanded ? boundsFor(workspace, state).removeFromTop(7.f) : Rectangle<float> {};
}

Rectangle<float> SignalProbeRail::collapseHandleFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    const Rectangle<float> rail = boundsFor(workspace, state);
    return state.expanded
            ? Rectangle<float>(rail.getRight() - 140.f, rail.getY() - 8.f, 116.f, 28.f)
            : Rectangle<float>(rail.getRight() - 174.f, rail.getY(), 150.f, rail.getHeight());
}

Rectangle<float> SignalProbeRail::refreshModeBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state) {
    if (!state.expanded) {
        return {};
    }
    const Rectangle<float> collapse = collapseHandleFor(workspace, state);
    return { collapse.getX() - 102.f, collapse.getY(), 94.f, collapse.getHeight() };
}

Rectangle<float> SignalProbeRail::tileBoundsFor(
        Rectangle<float> workspace,
        const SignalProbeRailState& state,
        int tileIndex) {
    Rectangle<float> rail = boundsFor(workspace, state);
    rail = rail.reduced(kRailPadding, 10.f);
    return {
            rail.getX() + (float) tileIndex * (kTileWidth + kTileGap) - state.horizontalOffset,
            rail.getY(),
            kTileWidth,
            rail.getHeight()
    };
}

float SignalProbeRail::maximumHorizontalOffset(
        Rectangle<float> workspace,
        int probeCount) {
    const float gaps = (float) jmax(0, probeCount - 1) * kTileGap;
    const float contentWidth = kRailPadding * 2.f + (float) probeCount * kTileWidth + gaps;
    return jmax(0.f, contentWidth - workspace.getWidth());
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
        return kMutedText;
    }

    const PortDomain domain = edge->attachment
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

Rectangle<float> SignalProbeRail::closeBounds(Rectangle<float> tile) {
    return Rectangle<float>(18.f, 18.f).withCentre({ tile.getRight() - 14.f, tile.getY() + 14.f });
}

String SignalProbeRail::probeAt(
        Point<float> position,
        Rectangle<float> workspace,
        const NodeGraph& graph,
        const SignalProbeRailState& state) const {
    if (!state.expanded) {
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

String SignalProbeRail::closeProbeAt(
        Point<float> position,
        Rectangle<float> workspace,
        const NodeGraph& graph,
        const SignalProbeRailState& state) const {
    if (!state.expanded) {
        return {};
    }
    const auto probes = orderedProbes(graph);
    for (int index = 0; index < (int) probes.size(); ++index) {
        if (closeBounds(tileBoundsFor(workspace, state, index)).contains(position)) {
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
        if (active && state.expanded) {
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
        graphics.setColour(Colour(0xff101318));
        graphics.fillEllipse(badge.withCentre(marker));
        graphics.setColour(colour);
        graphics.drawEllipse(badge.withCentre(marker), active ? 2.5f : 1.8f);
        graphics.setFont(FontOptions(9.f, Font::bold));
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
        const SignalProbeRailState& state) {
    const Rectangle<float> rail = boundsFor(workspace, state);
    graphics.setColour(kRailBackground);
    graphics.fillRect(rail);
    graphics.setColour(kRailBorder);
    graphics.drawHorizontalLine(roundToInt(rail.getY()), rail.getX(), rail.getRight());

    const auto probes = orderedProbes(graph);
    const Rectangle<float> collapse = collapseHandleFor(workspace, state);
    graphics.setColour(Colour(0xff26313d));
    graphics.fillRoundedRectangle(collapse, 6.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(12.f, Font::bold));
    graphics.drawText(
            state.expanded ? "Hide Spies" : "Spies (" + String((int) probes.size()) + ")",
            collapse,
            Justification::centred);

    if (!state.expanded) {
        return;
    }

    const Rectangle<float> refreshMode = refreshModeBoundsFor(workspace, state);
    graphics.setColour(Colour(0xff26313d));
    graphics.fillRoundedRectangle(refreshMode, 6.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(11.f));
    graphics.drawText(
            state.refreshMode == ProbeRefreshMode::LiveLatest ? "Live" : "On Release",
            refreshMode,
            Justification::centred);

    Graphics::ScopedSaveState tileClip(graphics);
    graphics.reduceClipRegion(rail.toNearestInt());
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Rectangle<float> tile = tileBoundsFor(workspace, state, index);
        const auto* preview = previewFor(previews, probe.id);
        const Colour colour = preview != nullptr && preview->connected
                ? colourForDomain(preview->domain)
                : kMutedText;
        const bool selected = probe.id == state.selectedProbeId;
        graphics.setColour(kTileBackground);
        graphics.fillRoundedRectangle(tile, 7.f);
        graphics.setColour(selected ? colour : kRailBorder);
        graphics.drawRoundedRectangle(tile, 7.f, selected ? 2.f : 1.f);

        Rectangle<float> tileContent = tile;
        Rectangle<float> header = tileContent.removeFromTop(28.f);
        graphics.setColour(colour);
        graphics.fillEllipse(Rectangle<float>(8.f, 8.f).withCentre({ header.getX() + 12.f, header.getCentreY() }));
        graphics.setColour(kText);
        graphics.setFont(FontOptions(12.f, Font::bold));
        graphics.drawText(
                String(index + 1) + "  " + probe.label,
                header.withTrimmedLeft(22.f).withTrimmedRight(28.f),
                Justification::centredLeft);
        const Rectangle<float> close = closeBounds(tile).reduced(5.f);
        graphics.setColour(kMutedText);
        graphics.drawLine(Line<float>(close.getTopLeft(), close.getBottomRight()), 1.4f);
        graphics.drawLine(Line<float>(close.getTopRight(), close.getBottomLeft()), 1.4f);

        Rectangle<float> previewBounds = tile.reduced(8.f).withTrimmedTop(22.f);
        if (preview == nullptr || !preview->connected) {
            graphics.setColour(kMutedText);
            graphics.drawText("Disconnected", previewBounds, Justification::centred);
            continue;
        }

        Node displayNode;
        displayNode.id = "probe-preview-" + probe.id;
        displayNode.kind = NodeKind::GenericProcessor;
        NodePreviewResult result {
                displayNode.id,
                PreviewModuleRole::SignalSpy,
                preview->values,
                {},
                preview->gridColumns,
                preview->gridRows,
                preview->domain
        };
        renderer.paint(graphics, {
                displayNode,
                &result,
                previewBounds,
                TrimeshRenderProfile::fromDomain(preview->domain),
                1.f,
                true
        });
    }
}

}
