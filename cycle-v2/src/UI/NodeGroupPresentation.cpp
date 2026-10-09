#include "UI/NodeGroupPresentation.h"

#include <cmath>

#include "Graph/GraphEdgeIndex.h"
#include "UI/NodeCanvasScene.h"

namespace CycleV2 {

namespace {

constexpr float kPadding = 19.f;
constexpr float kTitleHeight = 31.f;
constexpr float kCardWidth = 290.f;
constexpr float kCardHeight = 190.f;

juce::Rectangle<float> mapBounds(juce::Rectangle<float> card) {
    return card.reduced(13.f).withTrimmedTop(29.f).reduced(0.f, 4.f);
}

juce::Point<float> mappedCentre(
        juce::Rectangle<float> nodeBounds,
        juce::Rectangle<float> source,
        juce::Rectangle<float> destination) {
    return {
            juce::jmap(nodeBounds.getCentreX(), source.getX(), source.getRight(),
                    destination.getX(), destination.getRight()),
            juce::jmap(nodeBounds.getCentreY(), source.getY(), source.getBottom(),
                    destination.getY(), destination.getBottom())
    };
}

void paintGroupChrome(
        juce::Graphics& graphics,
        const NodeGroup& group,
        juce::Rectangle<float> bounds,
        juce::Rectangle<float> toggle) {
    const float radius = group.collapsed ? 9.f : 15.f;
    graphics.setColour(juce::Colour(0xff122332).withAlpha(group.collapsed ? 0.98f : 0.36f));
    graphics.fillRoundedRectangle(bounds, radius);
    graphics.setColour(juce::Colour(0xff63819a).withAlpha(group.collapsed ? 0.8f : 0.36f));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.f);

    graphics.setColour(juce::Colour(0xffb9ccdd));
    graphics.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
    graphics.drawFittedText(group.title + "  ·  " + juce::String((int) group.nodeIds.size()),
            bounds.withHeight(29.f).reduced(12.f, 0.f).toNearestInt(),
            juce::Justification::centredLeft, 1);

    graphics.setColour(juce::Colour(0xff182d3d));
    graphics.fillRoundedRectangle(toggle, 4.f);
    graphics.setColour(juce::Colour(0xffa8bdcf));
    graphics.drawRoundedRectangle(toggle, 4.f, 1.f);
    const auto centre = toggle.getCentre();
    graphics.drawLine(centre.x - 4.f, centre.y, centre.x + 4.f, centre.y, 1.5f);
    if (group.collapsed) {
        graphics.drawLine(centre.x, centre.y - 4.f, centre.x, centre.y + 4.f, 1.5f);
    }
}

void paintMapEdges(
        juce::Graphics& graphics,
        const NodeGraph& graph,
        const NodeGroup& group,
        const GraphEdgeIndex& edgeIndex,
        juce::Rectangle<float> source,
        juce::Rectangle<float> mapped) {
    graphics.setColour(juce::Colour(0xff7198b1).withAlpha(0.7f));
    for (const auto& id : group.nodeIds) {
        for (const size_t edgeIndexForNode : edgeIndex.outgoingEdges(id)) {
            const Edge& edge = graph.getEdges()[edgeIndexForNode];
            const NodeGroup* destinationGroup = graph.groupForNode(edge.destNodeId);
            if (destinationGroup == nullptr || destinationGroup->id != group.id) {
                continue;
            }
            const Node* from = graph.findNode(edge.sourceNodeId);
            const Node* to = graph.findNode(edge.destNodeId);
            if (from != nullptr && to != nullptr) {
                graphics.drawLine(juce::Line<float>(
                        mappedCentre(NodeCanvasScene::presentationWorldBounds(
                                graph, *from, edgeIndex), source, mapped),
                        mappedCentre(NodeCanvasScene::presentationWorldBounds(
                                graph, *to, edgeIndex), source, mapped)), 1.2f);
            }
        }
    }
}

void paintMapNodes(
        juce::Graphics& graphics,
        const NodeGraph& graph,
        const NodeGroup& group,
        juce::Rectangle<float> source,
        juce::Rectangle<float> mapped,
        juce::Rectangle<float> map,
        float scale,
        const GraphEdgeIndex& edgeIndex) {
    for (const auto& id : group.nodeIds) {
        if (const Node* node = graph.findNode(id)) {
            const auto presented = NodeCanvasScene::presentationWorldBounds(
                    graph, *node, edgeIndex);
            auto miniature = juce::Rectangle<float>(
                    juce::jmax(8.f, presented.getWidth() * scale),
                    juce::jmax(5.f, presented.getHeight() * scale))
                    .withCentre(mappedCentre(presented, source, mapped));
            miniature = miniature.getIntersection(map.reduced(2.f));
            graphics.setColour(juce::Colour(0xff41627b));
            graphics.fillRoundedRectangle(miniature, 2.f);
            graphics.setColour(juce::Colour(0xffa6c2d2));
            graphics.drawRoundedRectangle(miniature, 2.f, 0.8f);
        }
    }
}

}

juce::Rectangle<float> NodeGroupPresentation::memberWorldBounds(
        const NodeGraph& graph,
        const NodeGroup& group,
        const GraphEdgeIndex& edgeIndex) {
    juce::Rectangle<float> result;
    for (const auto& id : group.nodeIds) {
        if (const Node* node = graph.findNode(id)) {
            const auto presented = NodeCanvasScene::presentationWorldBounds(
                    graph, *node, edgeIndex);
            result = result.isEmpty() ? presented : result.getUnion(presented);
        }
    }
    return result;
}

juce::Rectangle<float> NodeGroupPresentation::worldBounds(
        const NodeGraph& graph,
        const NodeGroup& group,
        const GraphEdgeIndex& edgeIndex) {
    const auto members = memberWorldBounds(graph, group, edgeIndex);
    if (group.collapsed) {
        return { members.getX() - kPadding, members.getY() - kTitleHeight,
                kCardWidth, kCardHeight };
    }
    return members.expanded(kPadding).withTrimmedTop(-kTitleHeight + kPadding);
}

juce::Rectangle<float> NodeGroupPresentation::toggleBounds(
        const NodeGraph& graph,
        const NodeGroup& group,
        const NodeCanvasViewport& viewport,
        const GraphEdgeIndex& edgeIndex) {
    const auto bounds = viewport.toScreen(worldBounds(graph, group, edgeIndex));
    const float size = juce::jlimit(17.f, 26.f, 22.f * viewport.getZoom());
    return { bounds.getRight() - size - 7.f, bounds.getY() + 5.f, size, size };
}

juce::Point<float> NodeGroupPresentation::boundaryPoint(
        const NodeGraph& graph,
        const NodeGroup& group,
        juce::Point<float> other,
        const GraphEdgeIndex& edgeIndex) {
    const auto bounds = worldBounds(graph, group, edgeIndex);
    const auto centre = bounds.getCentre();
    const auto delta = other - centre;
    if (std::abs(delta.x) * bounds.getHeight()
            > std::abs(delta.y) * bounds.getWidth()) {
        return { delta.x < 0.f ? bounds.getX() : bounds.getRight(),
                juce::jlimit(bounds.getY() + 12.f, bounds.getBottom() - 12.f, other.y) };
    }
    return { juce::jlimit(bounds.getX() + 12.f, bounds.getRight() - 12.f, other.x),
            delta.y < 0.f ? bounds.getY() : bounds.getBottom() };
}

void NodeGroupPresentation::paint(
        juce::Graphics& graphics,
        const NodeGraph& graph,
        const NodeGroup& group,
        const NodeCanvasViewport& viewport,
        const GraphEdgeIndex& edgeIndex) {
    const auto bounds = viewport.toScreen(worldBounds(graph, group, edgeIndex));
    const auto toggle = toggleBounds(graph, group, viewport, edgeIndex);
    paintGroupChrome(graphics, group, bounds, toggle);
    if (!group.collapsed) {
        return;
    }

    const auto map = mapBounds(bounds);
    const auto source = memberWorldBounds(graph, group, edgeIndex).expanded(8.f);
    graphics.setColour(juce::Colour(0xff09151e));
    graphics.fillRoundedRectangle(map, 5.f);
    graphics.setColour(juce::Colour(0xff355066));
    graphics.drawRoundedRectangle(map, 5.f, 1.f);
    if (source.isEmpty()) {
        return;
    }

    const float scale = juce::jmin(map.getWidth() / source.getWidth(),
            map.getHeight() / source.getHeight());
    const auto mapped = juce::Rectangle<float>(
            source.getWidth() * scale, source.getHeight() * scale).withCentre(map.getCentre());
    paintMapEdges(graphics, graph, group, edgeIndex, source, mapped);
    paintMapNodes(graphics, graph, group, source, mapped, map, scale, edgeIndex);
}

}
