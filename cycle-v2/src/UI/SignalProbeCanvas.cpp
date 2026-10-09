#include <algorithm>

#include "UI/SignalProbeCanvas.h"

#include "Graph/GraphRenderSemanticResolver.h"

#include "UI/CanvasChromePalette.h"
#include "UI/NodeCableRenderer.h"
#include "UI/WorkspaceDock.h"

namespace CycleV2 {

namespace {

constexpr float kCableAnnotationBaseDiameter = 12.f;

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

float attachmentFraction(
        const SignalProbe& probe,
        const NodeGraph& graph,
        const NodeSceneEdge& sceneEdge) {
    if (!sceneEdge.inlinePan) {
        return jlimit(0.f, 1.f, probe.tapPosition);
    }
    if (sceneEdge.edgeIndices.empty()) {
        return 0.5f;
    }
    const Edge* edge = graphEdgeForProbe(probe, graph, sceneEdge);
    return edge == graphEdgeFor(graph, sceneEdge.edgeIndices.front())
            ? 1.f / 3.f
            : 2.f / 3.f;
}

void paintConnectionDot(
        Graphics& graphics,
        Point<float> centre,
        Colour colour,
        float diameter,
        bool active) {
    const Rectangle<float> dot = Rectangle<float>(diameter, diameter)
            .withCentre(centre);
    graphics.setColour(CanvasChromePalette::canvasBackground);
    graphics.fillEllipse(dot);
    graphics.setColour(colour.withAlpha(active ? 1.f : 0.88f));
    graphics.fillEllipse(dot.reduced(diameter * 0.21f));
    if (active) {
        graphics.drawEllipse(dot.expanded(diameter * 0.15f),
                jmax(1.f, diameter * 0.12f));
    }
}

}

bool SignalProbeCanvasState::isSelected(const String& probeId) const {
    return std::find(selectedProbeIds.begin(), selectedProbeIds.end(), probeId)
            != selectedProbeIds.end();
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
        return anchor->cablePath.getPointAlongPath(
                anchor->cablePath.getLength()
                        * attachmentFraction(probe, graph, *anchor));
    }

    const String sourceSemanticId = "output:" + probe.sourceNodeId + "." + probe.sourcePortId;
    const auto source = std::find_if(scene.targets.begin(), scene.targets.end(), [&](const auto& target) {
        return target.semanticId == sourceSemanticId;
    });
    return source != scene.targets.end() ? source->bounds.getCentre() : Point<float>();
}

Path SignalProbeCanvas::tetherPath(
        const Path& cable,
        float attachmentFraction,
        Point<float> target) {
    const float cableLength = cable.getLength();
    const float distance = cableLength * jlimit(0.f, 1.f, attachmentFraction);
    const Point<float> marker = cable.getPointAlongPath(distance);
    const float step = jmin(8.f, cableLength * 0.05f);
    const Point<float> before = cable.getPointAlongPath(jmax(0.f, distance - step));
    const Point<float> after = cable.getPointAlongPath(jmin(cableLength, distance + step));
    const Point<float> tangent = after - before;
    const float tangentLength = tangent.getDistanceFromOrigin();
    Point<float> normal = tangentLength > 0.001f
            ? Point<float>(-tangent.y, tangent.x) / tangentLength
            : Point<float>(0.f, -1.f);
    const Point<float> towardCard = target - marker;
    if (normal.x * towardCard.x + normal.y * towardCard.y < 0.f) {
        normal *= -1.f;
    }
    const float controlLength = jmin(35.f, towardCard.getDistanceFromOrigin() * 0.4f);
    Path tether;
    tether.startNewSubPath(marker);
    tether.cubicTo(marker + normal * controlLength,
            target.translated(0.f, 35.f), target);
    return tether;
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
    const float diameter = cableAnnotationDiameter(zoom);
    const auto paintAnnotation = [&](const SignalProbe& probe,
            Colour colour, bool active) {
        const NodeSceneEdge* anchor = anchorFor(probe, graph, scene);
        const float fraction = anchor != nullptr
                ? attachmentFraction(probe, graph, *anchor) : 0.5f;
        const Point<float> marker = anchor != nullptr
                ? anchor->cablePath.getPointAlongPath(
                        anchor->cablePath.getLength() * fraction)
                : markerCentre(probe, graph, scene);
        if (marker == Point<float>()) {
            return;
        }

        const Rectangle<float> card = cardBoundsFor(
                probe.id, graph, scene, viewport, state);
        if (!card.isEmpty()) {
            const Point<float> target = card.getCentre();
            Path tether = anchor != nullptr
                    ? tetherPath(anchor->cablePath, fraction, target)
                    : Path {};
            if (anchor == nullptr) {
                tether.startNewSubPath(marker);
                tether.lineTo(target);
            }
            graphics.setColour(colour.withAlpha(active ? 0.54f : 0.24f));
            graphics.strokePath(tether, PathStrokeType(active ? 2.f : 1.5f));
        }
        paintConnectionDot(graphics, marker, colour, diameter, active);
    };

    for (const SignalProbe* probe : orderedProbes(graph)) {
        const Colour colour = colourForProbe(*probe, graph, scene, facts);
        const bool active = probe->id == state.hoveredProbeId || state.isSelected(probe->id);
        paintAnnotation(*probe, colour, active);
    }

    const auto outputAddress = state.outputSpyVisible
            ? DefaultOutputProbeResolver().resolve(graph)
            : std::nullopt;
    if (outputAddress.has_value()) {
        SignalProbe outputProbe;
        outputProbe.sourceNodeId = outputAddress->sourceNodeId;
        outputProbe.sourcePortId = outputAddress->sourcePortId;
        outputProbe.anchorDestNodeId = outputAddress->destNodeId;
        outputProbe.anchorDestPortId = outputAddress->destPortId;
        outputProbe.tapPosition = 0.5f;
        outputProbe.id = DefaultOutputProbeResolver::probeId;
        const bool active = state.isSelected(outputProbe.id);
        paintAnnotation(outputProbe, colourForDomain(PortDomain::TimeSignal), active);
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
    if (defaultOutput && !state.outputSpyVisible) {
        return {};
    }
    const SignalProbe* probe = defaultOutput ? nullptr : graph.findSignalProbe(probeId);
    if (!defaultOutput && probe == nullptr) {
        return {};
    }
    const auto draggedPosition = std::find_if(
            state.draggedCardWorldPositions.begin(),
            state.draggedCardWorldPositions.end(),
            [&](const auto& entry) { return entry.first == probeId; });
    if (draggedPosition != state.draggedCardWorldPositions.end()) {
        return viewport.toScreen(cardSize.withPosition(
                draggedPosition->second
                        + state.draggedScreenOffset / viewport.getZoom()));
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
            outputProbe.tapPosition = 0.5f;
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
    if (probe.id == DefaultOutputProbeResolver::probeId
            || semantic.domain != preview.domain) {
        semantic = GraphRenderSemanticResolver::defaultSemanticForDomain(preview.domain);
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

        const bool active = state.hoveredProbeId == probeId || state.isSelected(probeId);
        WorkspaceDock::paintTileChrome(graphics, card, active, active, false);
        const GraphPreviewResult::SignalProbePreview* sourcePreview {};
        if (defaultOutput) {
            const auto& output = snapshot.previewResult.defaultOutput;
            sourcePreview = output.has_value() ? &*output : nullptr;
        } else {
            sourcePreview = facts.probePreviewFor(snapshot, probeId);
        }
        const float zoom = viewport.getZoom();
        const Rectangle<float> previewBounds = card.reduced(5.f * zoom);
        const GraphPreviewResult::SignalProbePreview* preview = sourcePreview;
        if (sourcePreview != nullptr
                && sourcePreview->domain == PortDomain::TimeSignal
                && (defaultOutput
                        ? state.defaultOutputView == PresetPreviewView::Spectrum
                        : probe->frequencyView)) {
            if (defaultOutput) {
                const auto& spectrum = snapshot.previewResult.defaultOutputSpectrum;
                preview = spectrum.has_value() ? &*spectrum : nullptr;
            } else {
                preview = facts.probeSpectrumFor(snapshot, probeId);
            }
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
