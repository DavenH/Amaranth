#include <algorithm>

#include "UI/SignalProbeCanvas.h"

#include "UI/CanvasChromePalette.h"
#include "UI/NodeCableRenderer.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

namespace {

constexpr float kCableAnnotationBaseDiameter = 23.04f;

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

int SignalProbeCanvas::ordinalForProbe(const NodeGraph& graph, const String& probeId) {
    const auto probes = orderedProbes(graph);
    if (probeId == DefaultOutputProbeResolver::probeId) {
        return (int) probes.size() + 1;
    }
    const auto found = std::find_if(probes.begin(), probes.end(), [&](const auto* probe) {
        return probe->id == probeId;
    });
    return found == probes.end()
            ? 0
            : (int) std::distance(probes.begin(), found) + 1;
}

std::vector<String> SignalProbeCanvas::orderedProbeIds(const NodeGraph& graph) {
    std::vector<String> ids;
    ids.reserve(graph.getSignalProbes().size() + 1);
    for (const auto* probe : orderedProbes(graph)) {
        ids.push_back(probe->id);
    }
    ids.push_back(DefaultOutputProbeResolver::probeId);
    return ids;
}

std::vector<const SignalProbe*> SignalProbeCanvas::orderedProbes(const NodeGraph& graph) {
    std::vector<const SignalProbe*> probes;
    probes.reserve(graph.getSignalProbes().size());
    for (const auto& probe : graph.getSignalProbes()) {
        probes.push_back(&probe);
    }
    std::stable_sort(probes.begin(), probes.end(), [](const auto* left, const auto* right) {
        return left->railOrder < right->railOrder;
    });
    return probes;
}

const NodeSceneEdge* SignalProbeCanvas::anchorFor(
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

Colour SignalProbeCanvas::colourForProbe(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const GraphPresentationFacts& facts) {
    const NodeSceneEdge* anchor = SignalProbeCanvas::anchorFor(probe, graph, scene);
    const Edge* edge = anchor == nullptr ? nullptr : graphEdgeForProbe(probe, graph, *anchor);
    if (edge == nullptr) {
        return CanvasChromePalette::mutedText;
    }

    const PortDomain domain = facts.domainForEdge(graph, *edge);
    return colourForDomain(domain);
}

Point<float> SignalProbeCanvas::markerCentre(
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

String SignalProbeCanvas::markerProbeAt(
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

void SignalProbeCanvas::paintCableAnnotations(
        Graphics& graphics,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const GraphPresentationFacts& facts,
        const NodeCanvasViewport& viewport,
        const SignalProbeCanvasState& state,
        float zoom) const {
    const auto probes = orderedProbes(graph);
    for (int index = 0; index < (int) probes.size(); ++index) {
        const SignalProbe& probe = *probes[(size_t) index];
        const Point<float> marker = markerCentre(probe, graph, scene);
        if (marker == Point<float>()) {
            continue;
        }

        const Colour colour = colourForProbe(probe, graph, scene, facts);
        const bool active = probe.id == state.hoveredProbeId || probe.id == state.selectedProbeId;
        const Rectangle<float> card = cardBoundsFor(
                probe.id, graph, scene, viewport, state);
        if (!card.isEmpty()) {
            const Point<float> target = card.getCentre();
            Path tether;
            tether.startNewSubPath(marker);
            tether.cubicTo(marker.translated(0.f, -35.f),
                    target.translated(0.f, 35.f), target);
            graphics.setColour(colour.withAlpha(active ? 0.54f : 0.24f));
            graphics.strokePath(tether, PathStrokeType(active ? 2.f : 1.5f));
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

    const auto outputAddress = DefaultOutputProbeResolver().resolve(graph);
    if (outputAddress.has_value()) {
        SignalProbe outputProbe;
        outputProbe.sourceNodeId = outputAddress->sourceNodeId;
        outputProbe.sourcePortId = outputAddress->sourcePortId;
        outputProbe.anchorDestNodeId = outputAddress->destNodeId;
        outputProbe.anchorDestPortId = outputAddress->destPortId;
        const Point<float> marker = markerCentre(outputProbe, graph, scene);
        const Rectangle<float> card = cardBoundsFor(
                DefaultOutputProbeResolver::probeId, graph, scene, viewport, state);
        if (marker != Point<float>() && !card.isEmpty()) {
            Path tether;
            tether.startNewSubPath(marker);
            tether.cubicTo(marker.translated(0.f, -35.f),
                    card.getCentre().translated(0.f, 35.f), card.getCentre());
            graphics.setColour(CanvasChromePalette::text.withAlpha(0.27f));
            graphics.strokePath(tether, PathStrokeType(1.5f));
            const float diameter = cableAnnotationDiameter(zoom);
            const Rectangle<float> badge(diameter, diameter);
            graphics.setColour(CanvasChromePalette::canvasBackground);
            graphics.fillEllipse(badge.withCentre(marker));
            graphics.setColour(CanvasChromePalette::text);
            graphics.drawEllipse(badge.withCentre(marker), 1.5f);
            graphics.setFont(FontOptions(9.f * diameter / 16.f));
            graphics.drawText("out", badge.withCentre(marker), Justification::centred);
        }
    }
}

float SignalProbeCanvas::cableAnnotationDiameter(float zoom) {
    return kCableAnnotationBaseDiameter * NodeCableRenderer::scaleForZoom(zoom);
}

Rectangle<float> SignalProbeCanvas::cardBoundsFor(
        const String& probeId,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const NodeCanvasViewport& viewport,
        const SignalProbeCanvasState& state) {
    const Rectangle<float> cardSize(0.f, 0.f, 300.f, 205.f);
    const bool defaultOutput = probeId == DefaultOutputProbeResolver::probeId;
    const SignalProbe* probe = defaultOutput ? nullptr : graph.findSignalProbe(probeId);
    if (!defaultOutput && probe == nullptr) {
        return {};
    }
    if (state.draggedProbeId == probeId && state.draggedCanvasPosition.has_value()) {
        return viewport.toScreen(cardSize.withPosition(*state.draggedCanvasPosition));
    }
    if (defaultOutput && state.outputCanvasPosition.has_value()) {
        return viewport.toScreen(cardSize.withPosition(*state.outputCanvasPosition));
    }
    if (probe != nullptr && probe->canvasPosition.has_value()) {
        return viewport.toScreen(cardSize.withPosition(*probe->canvasPosition));
    }

    SignalProbe outputProbe;
    if (defaultOutput) {
        const auto address = DefaultOutputProbeResolver().resolve(graph);
        if (address.has_value()) {
            outputProbe.sourceNodeId = address->sourceNodeId;
            outputProbe.sourcePortId = address->sourcePortId;
            outputProbe.anchorDestNodeId = address->destNodeId;
            outputProbe.anchorDestPortId = address->destPortId;
        }
    }
    const auto visibleDefaultCard = [&](Rectangle<float> candidate) {
        const Rectangle<float> canvas = viewport.getBounds();
        const float leftClearance = jmin(272.f, canvas.getWidth() * 0.256f) + 28.f;
        return candidate.withPosition(
                jlimit(canvas.getX() + leftClearance,
                        jmax(canvas.getX() + leftClearance,
                                canvas.getRight() - candidate.getWidth() - 18.f),
                        candidate.getX()),
                jlimit(canvas.getY() + 145.f,
                        jmax(canvas.getY() + 145.f,
                                canvas.getBottom() - candidate.getHeight() - 18.f),
                        candidate.getY()));
    };
    const Point<float> marker = markerCentre(defaultOutput ? outputProbe : *probe, graph, scene);
    if (marker != Point<float>()) {
        return visibleDefaultCard(viewport.toScreen(cardSize.withPosition(
                viewport.toWorld(marker)
                        + (defaultOutput
                                ? Point<float>(360.f, -242.f)
                                : Point<float>(52.f, -242.f)))));
    }
    const auto output = std::find_if(graph.getNodes().begin(), graph.getNodes().end(),
            [](const Node& node) { return node.kind == NodeKind::Output; });
    if (defaultOutput && output != graph.getNodes().end()) {
        return visibleDefaultCard(viewport.toScreen(cardSize.withPosition(
                output->bounds.getPosition() + Point<float>(58.f, -245.f))));
    }
    return {};
}

String SignalProbeCanvas::cardAt(
        Point<float> position,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const NodeCanvasViewport& viewport,
        const SignalProbeCanvasState& state) {
    const auto ids = orderedProbeIds(graph);
    for (auto id = ids.rbegin(); id != ids.rend(); ++id) {
        if (cardBoundsFor(*id, graph, scene, viewport, state).contains(position)) {
            return *id;
        }
    }
    return {};
}

void SignalProbeCanvas::paintCachedPreview(
        Graphics& graphics,
        const NodeGraph& graph,
        const SignalProbe& probe,
        const GraphPreviewResult::SignalProbePreview& preview,
        const GraphPresentationFacts& facts,
        Rectangle<float> previewBounds,
        float physicalScale) {
    NodeRenderSemantic semantic = facts.renderSemanticForNodeOutput(
            graph, probe.sourceNodeId, probe.sourcePortId);
    if (probe.id == DefaultOutputProbeResolver::probeId) {
        const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(preview.domain);
        semantic.domain = preview.domain;
        semantic.scalePolicy = profile.getScalePolicy();
    } else if (semantic.domain == PortDomain::ControlSignal) {
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
                -(float) logicalBounds.getX() * physicalScale,
                0.f,
                physicalScale,
                -(float) logicalBounds.getY() * physicalScale));
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

void SignalProbeCanvas::paintCards(
        Graphics& graphics,
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const NodeCanvasViewport& viewport,
        const GraphPresentationSnapshot& snapshot,
        const GraphPresentationFacts& facts,
        const SignalProbeCanvasState& state) {
    const auto ids = orderedProbeIds(graph);
    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    uint64_t previewElapsed {};
    previewTileCache.beginFrame();
    for (int index = 0; index < (int) ids.size(); ++index) {
        const String& probeId = ids[(size_t) index];
        const bool defaultOutput = probeId == DefaultOutputProbeResolver::probeId;
        const Rectangle<float> card = cardBoundsFor(
                probeId, graph, scene, viewport, state);
        if (card.isEmpty()) {
            continue;
        }
        const SignalProbe* probe = graph.findSignalProbe(probeId);
        SignalProbe outputProbe;
        if (defaultOutput) {
            outputProbe.id = probeId;
            const auto address = DefaultOutputProbeResolver().resolve(graph);
            if (address.has_value()) {
                outputProbe.sourceNodeId = address->sourceNodeId;
                outputProbe.sourcePortId = address->sourcePortId;
            }
            probe = &outputProbe;
        }
        if (probe == nullptr) {
            continue;
        }

        const bool active = state.hoveredProbeId == probeId
                || state.selectedProbeId == probeId;
        WorkspaceDock::paintTileChrome(graphics, card, active, active, false);
        const float headerHeight = 25.f * viewport.getZoom();
        Rectangle<float> header = card.withHeight(headerHeight);
        graphics.setColour(CanvasChromePalette::text);
        graphics.setFont(FontOptions(jmax(10.f, 15.f * viewport.getZoom())));
        graphics.drawText(defaultOutput ? "Output Spy" : "Spy " + String(index + 1),
                header.reduced(7.f, 0.f), Justification::centredLeft);

        const Rectangle<float> previewBounds = card.withTrimmedTop(headerHeight)
                .reduced(5.f * viewport.getZoom());
        const GraphPreviewResult::SignalProbePreview* preview {};
        if (defaultOutput) {
            const auto& selected = state.defaultOutputView == PresetPreviewView::Time
                    ? snapshot.previewResult.defaultOutput
                    : snapshot.previewResult.defaultOutputSpectrum;
            preview = selected.has_value() ? &*selected : nullptr;
        } else {
            preview = facts.probePreviewFor(snapshot, probeId);
        }
        if (preview == nullptr || !preview->connected) {
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.drawText("Disconnected", previewBounds, Justification::centred);
            continue;
        }
        const uint64_t startedAt = performanceObserver != nullptr
                ? performanceObserver->presentationTimestamp()
                : 0;
        paintCachedPreview(graphics, graph, *probe, *preview,
                facts, previewBounds, physicalScale);
        if (performanceObserver != nullptr) {
            previewElapsed += performanceObserver->presentationTimestamp() - startedAt;
        }
    }
    const auto stats = previewTileCache.endFrame();
    if (performanceObserver != nullptr) {
        performanceObserver->presentationStageCompleted(
                NodeCanvasPresentationStage::SpyCanvasPreviews, previewElapsed);
        performanceObserver->spyPreviewTileCacheCompleted(
                stats.hits, stats.misses, previewElapsed);
    }
}

}
