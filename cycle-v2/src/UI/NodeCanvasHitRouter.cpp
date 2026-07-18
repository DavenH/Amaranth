#include "NodeCanvasHitRouter.h"

#include "NodeViewModule.h"
#include "VoiceContextCompactEditor.h"
#include "../Graph/GraphNodeFactory.h"

namespace CycleV2 {

namespace {

Rectangle<float> nodeActionBounds(Rectangle<float> nodeBounds, float zoom) {
    const float size = 27.f * zoom;
    return Rectangle<float>(size, size)
            .withCentre({ nodeBounds.getRight() - 21.f * zoom, nodeBounds.getY() + 21.f * zoom });
}

bool isOperationNode(NodeKind kind) {
    return NodeViewModuleRegistry::instance()
            .moduleFor(kind)
            .capabilities()
            .operationLayoutControl;
}

bool hasOutputSideControl(NodeKind kind) {
    return NodeViewModuleRegistry::instance()
            .moduleFor(kind)
            .capabilities()
            .outputSideControl;
}

std::optional<CanvasNodeActionKind> actionKindForNode(NodeKind kind) {
    if (isOperationNode(kind)) {
        return CanvasNodeActionKind::CycleOperationLayout;
    }

    if (hasOutputSideControl(kind)) {
        return CanvasNodeActionKind::CycleMeshOutputSide;
    }

    if (kind == NodeKind::VoiceContext) {
        return CanvasNodeActionKind::CycleVoiceDomain;
    }

    return std::nullopt;
}

bool actionContains(
        CanvasNodeActionKind kind,
        Rectangle<float> nodeBounds,
        float zoom,
        Point<float> screenPosition) {
    if (kind == CanvasNodeActionKind::CycleVoiceDomain) {
        return VoiceContextCompactEditor::hitNodeSelector(
                nodeBounds,
                zoom,
                screenPosition);
    }

    return nodeActionBounds(nodeBounds, zoom)
            .expanded(4.f * zoom)
            .contains(screenPosition);
}

String hoverTextForAction(const CanvasNodeAction& action, const NodeCanvasQueryModel& queries) {
    switch (action.kind) {
        case CanvasNodeActionKind::CycleOperationLayout:
            return "Cycle operation port layout  /  side, uptack, vertical, T";

        case CanvasNodeActionKind::CycleMeshOutputSide:
            return "Cycle output side  /  east, south, north";

        case CanvasNodeActionKind::CycleVoiceDomain:
            if (const Node* node = queries.findNode(action.nodeId)) {
                return "Voice start domain  /  " + VoiceContextCompactEditor::domainLabel(*node)
                        + "  /  click to switch";
            }
            return {};
    }

    return {};
}

}

NodeCanvasHitRouter::NodeCanvasHitRouter(
        const NodeGraph& targetGraph,
        const NodePalette& targetPalette,
        const NodeCanvasQueryModel& targetQueries)
    :   graph(targetGraph)
    ,   palette(targetPalette)
    ,   queries(targetQueries) {
}

std::optional<CanvasNodeAction> NodeCanvasHitRouter::nodeActionAt(
        const NodeCanvasViewport& viewport,
        Point<float> screenPosition) const {
    const float zoom = viewport.getZoom();
    const auto& nodes = graph.getNodes();

    for (int i = (int) nodes.size() - 1; i >= 0; --i) {
        const auto& node = nodes[(size_t) i];
        const auto actionKind = actionKindForNode(node.kind);

        if (actionKind.has_value()
                && actionContains(
                        *actionKind,
                        viewport.toScreen(node.bounds),
                        zoom,
                        screenPosition)) {
            return CanvasNodeAction { *actionKind, node.id };
        }
    }

    return std::nullopt;
}

int NodeCanvasHitRouter::edgeAt(
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition) const {
    const auto hit = hitTester.hitTest(scene, screenPosition);
    return hit.has_value() && hit->kind == NodeSceneTargetKind::Edge
            ? hit->edgeIndex
            : -1;
}

int NodeCanvasHitRouter::spliceTargetEdgeAt(
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition,
        const String& nodeId) const {
    const auto& edges = graph.getEdges();
    for (auto sceneEdge = scene.edges.rbegin(); sceneEdge != scene.edges.rend(); ++sceneEdge) {
        const int edgeIndex = sceneEdge->edgeIndex;

        if (edgeIndex < 0 || edgeIndex >= (int) edges.size()) {
            continue;
        }

        const auto& edge = edges[(size_t) edgeIndex];

        if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId
                || !sceneEdge->hitPath.contains(screenPosition)) {
            continue;
        }

        NodeGraph candidate = graph;
        const auto result = GraphEditor().spliceNodeIntoEdge(candidate, (size_t) edgeIndex, nodeId);

        if (result.succeeded()) {
            return edgeIndex;
        }
    }

    return -1;
}

String NodeCanvasHitRouter::hoverTextFor(
        const NodeCanvasViewport& viewport,
        const NodeCanvasSceneSnapshot& scene,
        Point<float> screenPosition) const {
    NodeKind paletteKind;

    if (palette.findKindAt(screenPosition, paletteKind)) {
        const Node node = GraphNodeFactory().createNode(paletteKind, {}, {});
        return "Create " + node.title + "  /  " + node.subtitle;
    }

    const int paletteSectionIndex = palette.findSectionAt(screenPosition);
    if (paletteSectionIndex >= 0) {
        return "Node group  /  " + String(palette.section(paletteSectionIndex).title);
    }

    if (const auto action = nodeActionAt(viewport, screenPosition)) {
        return hoverTextForAction(*action, queries);
    }

    const auto hit = hitTester.hitTest(scene, screenPosition);
    if (!hit.has_value()) {
        return {};
    }

    if (hit->isPort()) {
        return queries.hoverTextForPort(hit->portAddress());
    }

    if (hit->kind == NodeSceneTargetKind::Node) {
        if (const Node* node = queries.findNode(hit->nodeId)) {
            return queries.hoverTextForNode(*node);
        }
    }

    if (hit->kind == NodeSceneTargetKind::Edge
            && hit->edgeIndex >= 0
            && hit->edgeIndex < (int) graph.getEdges().size()) {
        return queries.hoverTextForEdge(graph.getEdges()[(size_t) hit->edgeIndex]);
    }

    return {};
}

Point<float> NodeCanvasHitRouter::paletteCreationWorldPosition(
        const NodeCanvasViewport& viewport,
        Rectangle<float> canvasBounds,
        NodeKind kind,
        Point<float> paletteClickPosition) const {
    const float paletteRight = palette.railBounds().getRight();
    const float x = jmin(canvasBounds.getRight() - 280.f, paletteRight + 32.f);
    Point<float> position = viewport.toWorld(Point<float> { x, paletteClickPosition.y });

    if (isOperationNode(kind)) {
        const Node node = GraphNodeFactory().createNode(kind, {}, position);

        if (!node.inputs.empty()) {
            const float inputOffset = NodeCanvasScene::portWorldCentre(
                    node,
                    node.inputs.front()).y - node.bounds.getY();
            position.y -= inputOffset;
        }
    }

    return position;
}

Rectangle<float> NodeCanvasHitRouter::paletteDragBounds(
        const NodeCanvasViewport& viewport,
        const Node& node,
        Point<float> paletteClickPosition) const {
    if (!isOperationNode(node.kind) || node.inputs.empty()) {
        return node.bounds;
    }

    const Point<float> mouseWorld = viewport.toWorld(paletteClickPosition);
    const Point<float> inputCentre = NodeCanvasScene::portWorldCentre(node, node.inputs.front());
    return node.bounds.translated(
            mouseWorld.x - inputCentre.x,
            mouseWorld.y - inputCentre.y);
}

}
