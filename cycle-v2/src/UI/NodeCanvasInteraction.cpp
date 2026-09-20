#include "UI/NodeCanvasInteraction.h"

#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphConnectionValidator.h"
#include "UI/ModulationCableBundle.h"

#include <algorithm>
#include <limits>

namespace CycleV2 {

namespace {

constexpr float kSnapBaseDistance = 9.f;
constexpr float kSnapExtraPerMatch = 2.5f;
constexpr float kSnapMaxDistance = 20.f;
constexpr float kConnectionSnapExpansion = 50.f;

struct SnapCandidate {
    float target {};
    int matches { 1 };
};

float absoluteFloat(float value) {
    return value >= 0.f ? value : -value;
}

float snapDistance(int matches) {
    return jmin(
            kSnapMaxDistance,
            kSnapBaseDistance + (float) jmax(0, matches - 1) * kSnapExtraPerMatch);
}

void addCandidate(std::vector<SnapCandidate>& candidates, float target) {
    constexpr float mergeDistance = 0.5f;

    for (auto& candidate : candidates) {
        if (absoluteFloat(candidate.target - target) > mergeDistance) {
            continue;
        }

        candidate.target = (candidate.target * (float) candidate.matches + target)
                / (float) (candidate.matches + 1);
        ++candidate.matches;
        return;
    }

    candidates.push_back({ target, 1 });
}

void addNodeCandidates(
        std::vector<SnapCandidate>& xCandidates,
        std::vector<SnapCandidate>& yCandidates,
        const Node& node) {
    auto addPorts = [&](const std::vector<Port>& ports) {
        for (const auto& port : ports) {
            const Point<float> centre = NodeCanvasScene::portWorldCentre(node, port);
            addCandidate(xCandidates, centre.x);
            addCandidate(yCandidates, centre.y);
        }
    };

    addPorts(node.inputs);
    addPorts(node.outputs);
}

void addDraggedAnchors(
        std::vector<float>& xAnchors,
        std::vector<float>& yAnchors,
        const Node& node,
        Rectangle<float> bounds) {
    Node positioned = node;
    positioned.bounds = bounds;

    auto addPorts = [&](const std::vector<Port>& ports) {
        for (const auto& port : ports) {
            const Point<float> centre = NodeCanvasScene::portWorldCentre(positioned, port);
            xAnchors.push_back(centre.x);
            yAnchors.push_back(centre.y);
        }
    };

    addPorts(positioned.inputs);
    addPorts(positioned.outputs);
}

std::optional<float> bestSnapDelta(
        const std::vector<float>& anchors,
        const std::vector<SnapCandidate>& candidates,
        float& guide) {
    float bestDistance = std::numeric_limits<float>::max();
    float bestDelta {};
    int bestMatches {};

    for (const float anchor : anchors) {
        for (const auto& candidate : candidates) {
            const float delta = candidate.target - anchor;
            const float distance = absoluteFloat(delta);
            const float threshold = snapDistance(candidate.matches);

            if (distance > threshold
                    || (distance > bestDistance)
                    || (distance == bestDistance && candidate.matches <= bestMatches)) {
                continue;
            }

            bestDistance = distance;
            bestDelta = delta;
            bestMatches = candidate.matches;
            guide = candidate.target;
        }
    }

    return bestMatches > 0 ? std::optional<float>(bestDelta) : std::nullopt;
}

bool canConnect(
        const NodeGraph& graph,
        const GraphValidationContext& context,
        const PortAddress& source,
        const PortAddress& target) {
    if (source.input == target.input) {
        return false;
    }

    if (ModulationCableBundle::isAddress(source)
            || ModulationCableBundle::isAddress(target)) {
        return ModulationCableBundle::canConnect(graph, context, source, target);
    }

    return GraphConnectionValidator().validate(graph, context, source, target).succeeded();
}

}

void NodeCanvasInteraction::beginPan(Point<float> startPan) {
    currentGesture = CanvasPanGesture { startPan };
}

void NodeCanvasInteraction::beginAreaSelection(Point<float> start) {
    currentGesture = AreaSelectionGesture { start, start };
}

void NodeCanvasInteraction::beginNodeDrag(
        const NodeGraph& graph,
        const String& nodeId,
        std::vector<String> nodeIds,
        Rectangle<float> startBounds) {
    if (std::find(nodeIds.begin(), nodeIds.end(), nodeId) == nodeIds.end()) {
        nodeIds.push_back(nodeId);
    }
    currentGesture = NodeDragGesture { nodeId, std::move(nodeIds), startBounds, {} };
    validationContext = std::make_unique<GraphValidationContext>(graph);
}

void NodeCanvasInteraction::beginConnection(
        const NodeGraph& graph,
        const PortAddress& source,
        Point<float> endpoint) {
    currentGesture = PortConnectionGesture { source, endpoint };
    validationContext = std::make_unique<GraphValidationContext>(graph);
}

void NodeCanvasInteraction::beginSpectralPan(const String& nodeId, float startValue) {
    currentGesture = SpectralPanGesture { nodeId, startValue };
}

void NodeCanvasInteraction::beginOutputGain(const String& nodeId, float startValue) {
    currentGesture = OutputGainGesture { nodeId, startValue };
}

void NodeCanvasInteraction::beginProbeDrag(const String& probeId) {
    currentGesture = ProbeDragGesture { probeId };
}

void NodeCanvasInteraction::captureExpandedEditor() {
    currentGesture = ExpandedEditorGesture {};
}

void NodeCanvasInteraction::reset() {
    currentGesture = std::monostate {};
    validationContext.reset();
}

bool NodeCanvasInteraction::isIdle() const {
    return std::holds_alternative<std::monostate>(currentGesture);
}

const SpectralPanGesture* NodeCanvasInteraction::spectralPan() const {
    return std::get_if<SpectralPanGesture>(&currentGesture);
}

const OutputGainGesture* NodeCanvasInteraction::outputGain() const {
    return std::get_if<OutputGainGesture>(&currentGesture);
}

const ProbeDragGesture* NodeCanvasInteraction::probeDrag() const {
    return std::get_if<ProbeDragGesture>(&currentGesture);
}

std::optional<NodeSceneTarget> NodeCanvasInteraction::hitAt(
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition) const {
    return hitTester.hitTest(scene, screenPosition);
}

std::optional<PortAddress> NodeCanvasInteraction::portAt(
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition) const {
    const auto hit = hitAt(scene, screenPosition);
    return hit.has_value() && hit->isPort()
            ? std::optional<PortAddress>(hit->portAddress())
            : std::nullopt;
}

std::optional<PortAddress> NodeCanvasInteraction::connectionTargetAt(
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        const PortAddress& source,
        Point<float> screenPosition) const {
    float bestDistance = std::numeric_limits<float>::max();
    std::optional<PortAddress> bestTarget;
    std::unique_ptr<GraphValidationContext> localContext;
    const GraphValidationContext* context = validationContext.get();
    if (context == nullptr || !context->matches(graph)) {
        localContext = std::make_unique<GraphValidationContext>(graph);
        context = localContext.get();
    }

    for (const auto& target : scene.targets) {
        if (!target.isPort() || !target.bounds.expanded(kConnectionSnapExpansion).contains(screenPosition)) {
            continue;
        }

        const PortAddress candidate = target.portAddress();
        if (!canConnect(graph, *context, source, candidate)) {
            continue;
        }

        const float distance = screenPosition.getDistanceFrom(target.bounds.getCentre());
        if (distance < bestDistance) {
            bestDistance = distance;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

std::vector<String> NodeCanvasInteraction::nodeIdsIntersecting(
        const NodeGraph& graph,
        const NodeCanvasViewport& viewport,
        Rectangle<float> screenBounds,
        const GraphEdgeIndex& edgeIndex) const {
    std::vector<String> result;
    for (const auto& node : graph.getNodes()) {
        const Rectangle<float> nodeBounds = viewport.toScreen(
                NodeCanvasScene::presentationWorldBounds(graph, node, edgeIndex));
        if (screenBounds.intersects(nodeBounds)) {
            result.push_back(node.id);
        }
    }
    return result;
}

SnappedNodeBounds NodeCanvasInteraction::snapNode(
        const NodeGraph& graph,
        const Node& node,
        Rectangle<float> proposed,
        const std::vector<String>& excludedNodeIds) const {
    std::vector<SnapCandidate> xCandidates;
    std::vector<SnapCandidate> yCandidates;
    std::vector<float> xAnchors;
    std::vector<float> yAnchors;

    for (const auto& candidate : graph.getNodes()) {
        if (std::find(
                    excludedNodeIds.begin(),
                    excludedNodeIds.end(),
                    candidate.id) == excludedNodeIds.end()) {
            addNodeCandidates(xCandidates, yCandidates, candidate);
        }
    }

    addDraggedAnchors(xAnchors, yAnchors, node, proposed);

    NodeSnapGuides guides;
    float xGuide {};
    if (const auto delta = bestSnapDelta(xAnchors, xCandidates, xGuide)) {
        proposed = proposed.translated(*delta, 0.f);
        guides.x = xGuide;
    }

    xAnchors.clear();
    yAnchors.clear();
    addDraggedAnchors(xAnchors, yAnchors, node, proposed);

    float yGuide {};
    if (const auto delta = bestSnapDelta(yAnchors, yCandidates, yGuide)) {
        proposed = proposed.translated(0.f, *delta);
        guides.y = yGuide;
    }

    return { proposed, guides };
}

NodeCanvasDragUpdate NodeCanvasInteraction::drag(
        const NodeGraph& graph,
        const NodeCanvasViewport& viewport,
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition,
        Point<float> dragOffset) {
    if (auto* pan = std::get_if<CanvasPanGesture>(&currentGesture)) {
        return PanDragUpdate { pan->startPan + dragOffset };
    }

    if (auto* selection = std::get_if<AreaSelectionGesture>(&currentGesture)) {
        selection->current = screenPosition;
        selection->moved = dragOffset.getDistanceFromOrigin() > 3.f;
        return AreaSelectionDragUpdate {
                selection->bounds(),
                selection->moved
        };
    }

    if (auto* connection = std::get_if<PortConnectionGesture>(&currentGesture)) {
        const auto target = connectionTargetAt(graph, scene, connection->source, screenPosition);
        connection->endpoint = screenPosition;
        if (target.has_value()) {
            for (const auto& sceneTarget : scene.targets) {
                if (sceneTarget.isPort()
                        && sceneTarget.portAddress().nodeId == target->nodeId
                        && sceneTarget.portAddress().portId == target->portId
                        && sceneTarget.portAddress().input == target->input) {
                    connection->endpoint = sceneTarget.bounds.getCentre();
                    break;
                }
            }
        }
        return ConnectionDragUpdate { connection->source, connection->endpoint, target };
    }

    auto* nodeDrag = std::get_if<NodeDragGesture>(&currentGesture);
    if (nodeDrag == nullptr) {
        return {};
    }

    const Node* node = graph.findNode(nodeDrag->nodeId);
    if (node == nullptr) {
        return {};
    }

    nodeDrag->moved = dragOffset.getDistanceFromOrigin() > 3.f;
    const Point<float> worldOffset = dragOffset / viewport.getZoom();
    const auto snapped = snapNode(
            graph,
            *node,
            nodeDrag->startBounds.translated(worldOffset.x, worldOffset.y),
            nodeDrag->nodeIds);
    nodeDrag->guides = snapped.guides;
    const bool beginTransaction = !nodeDrag->transactionRequested;
    nodeDrag->transactionRequested = true;
    return NodeDragUpdate {
            nodeDrag->nodeId,
            nodeDrag->nodeIds,
            snapped.bounds,
            snapped.guides,
            beginTransaction,
            nodeDrag->moved
    };
}

NodeCanvasGestureCompletion NodeCanvasInteraction::finish(
        const NodeGraph& graph,
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition) {
    NodeCanvasGestureCompletion completion;

    if (const auto* selection = std::get_if<AreaSelectionGesture>(&currentGesture)) {
        AreaSelectionGesture finished = *selection;
        finished.current = screenPosition;
        completion = AreaSelectionCompletion { finished.bounds(), finished.moved };
    } else if (const auto* nodeDrag = std::get_if<NodeDragGesture>(&currentGesture)) {
        completion = NodeDragCompletion {
                nodeDrag->nodeId,
                nodeDrag->nodeIds,
                nodeDrag->moved
        };
    } else if (const auto* connection = std::get_if<PortConnectionGesture>(&currentGesture)) {
        completion = ConnectionCompletion {
                connection->source,
                connectionTargetAt(graph, scene, connection->source, screenPosition)
        };
    }

    reset();
    return completion;
}

}
