#include "UI/GroupInformationCardPresentation.h"

#include "UI/CanvasChromePalette.h"
#include "UI/NodeGroupPresentation.h"

namespace CycleV2 {

namespace {

constexpr float width = 300.f;
constexpr float inset = 10.f;
constexpr float headerHeight = 26.f;
constexpr float maximumBodyHeight = 136.f;

}

void GroupInformationCardPresentation::update(
        const NodeGraph& graph, const GraphEdgeIndex& edges, uint64_t revision) {
    if (cachedGraph == &graph && graphRevision == graph.getRevision()
            && documentRevision == revision) {
        return;
    }
    cachedGraph = &graph;
    graphRevision = graph.getRevision();
    documentRevision = revision;
    visible.clear();
    for (auto item = cards.begin(); item != cards.end();) {
        if (graph.findNodeGroup(item->first) == nullptr) {
            item = cards.erase(item);
        } else {
            ++item;
        }
    }
    for (const auto& group : graph.getNodeGroups()) {
        if (!group.informationCardOffset.has_value() || group.rationale.isEmpty()) {
            continue;
        }
        auto& card = cards[group.id];
        if (card.text != group.rationale) {
            card.text = group.rationale;
            juce::AttributedString text;
            text.append(card.text, juce::Font(juce::FontOptions(14.f)), CanvasChromePalette::text);
            card.layout.createLayout(text, width - 2.f * inset);
            const int lastLine = juce::jmin(8, card.layout.getNumLines()) - 1;
            const float bodyHeight = lastLine >= 0 ? juce::jmin(maximumBodyHeight,
                    card.layout.getLine(lastLine).getLineBounds().getBottom()) : 0.f;
            card.height = headerHeight + 2.f * inset + bodyHeight;
        }
        card.groupId = group.id;
        card.title = group.title;
        card.origin = NodeGroupPresentation::memberWorldBounds(graph, group, edges).getPosition();
        card.offset = *group.informationCardOffset;
        visible.push_back(&card);
    }
}

const GroupInformationCard* GroupInformationCardPresentation::find(const juce::String& id) const {
    for (const auto* card : visible) {
        if (card->groupId == id) {
            return card;
        }
    }
    return nullptr;
}

GroupInformationCardBounds GroupInformationCardPresentation::bounds(
        const GroupInformationCard& card, const NodeCanvasViewport& viewport,
        const std::optional<GroupInformationCardPreview>& preview) {
    const auto offset = preview.has_value() && preview->groupId == card.groupId
            ? preview->offset : card.offset;
    const auto world = juce::Rectangle<float>(width, card.height).withPosition(card.origin + offset);
    const auto screen = viewport.toScreen(world);
    const float zoom = viewport.getZoom();
    const auto header = screen.withHeight(headerHeight * zoom);
    return { screen, header,
            screen.reduced(inset * zoom).withTrimmedTop(headerHeight * zoom),
            header.withLeft(header.getRight() - 26.f * zoom) };
}

const GroupInformationCard* GroupInformationCardPresentation::cardAt(
        juce::Point<float> position, const NodeCanvasViewport& viewport,
        const std::optional<GroupInformationCardPreview>& preview) const {
    for (auto card = visible.rbegin(); card != visible.rend(); ++card) {
        if (bounds(**card, viewport, preview).card.contains(position)) {
            return *card;
        }
    }
    return nullptr;
}

juce::Point<float> GroupInformationCardPresentation::defaultOffset(
        const NodeGraph& graph, const NodeGroup& group, const GraphEdgeIndex& edges) {
    return { 0.f, NodeGroupPresentation::memberWorldBounds(graph, group, edges).getHeight() + 36.f };
}

void GroupInformationCardPresentation::paint(
        juce::Graphics& graphics, const NodeCanvasViewport& viewport,
        const juce::String& focusedGroup,
        const std::optional<GroupInformationCardPreview>& preview) const {
    for (const auto* card : visible) {
        const auto area = bounds(*card, viewport, preview);
        const float zoom = viewport.getZoom();
        juce::Graphics::ScopedSaveState state(graphics);
        graphics.setOpacity(1.f);
        graphics.setColour(CanvasChromePalette::raisedSurface);
        graphics.fillRect(area.card);
        graphics.setColour(focusedGroup == card->groupId
                ? CanvasChromePalette::focus : CanvasChromePalette::border);
        graphics.drawRect(area.card, juce::jmax(0.8f, zoom));
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.setFont(juce::FontOptions(13.f * zoom, juce::Font::bold));
        graphics.drawText(card->title, area.header.reduced(inset * zoom, 0.f)
                .withTrimmedRight(26.f * zoom), juce::Justification::centredLeft, true);
        const auto close = area.close.reduced(9.f * zoom);
        graphics.drawLine(close.getX(), close.getY(), close.getRight(), close.getBottom(), zoom);
        graphics.drawLine(close.getRight(), close.getY(), close.getX(), close.getBottom(), zoom);
        graphics.reduceClipRegion(area.body.toNearestInt());
        graphics.addTransform(juce::AffineTransform::scale(zoom)
                .translated(area.body.getX(), area.body.getY()));
        card->layout.draw(graphics, { 0.f, 0.f, width - 2.f * inset, card->layout.getHeight() });
        const float bodyHeight = card->height - headerHeight - 2.f * inset;
        if (card->layout.getHeight() > bodyHeight + 0.5f) {
            graphics.setColour(CanvasChromePalette::raisedSurface);
            graphics.fillRect(width - 44.f, bodyHeight - 17.f, 24.f, 17.f);
            graphics.setColour(CanvasChromePalette::mutedText);
            graphics.setFont(juce::FontOptions(14.f));
            graphics.drawText(juce::String::charToString(0x2026), juce::Rectangle<float>(width - 44.f, bodyHeight - 17.f, 24.f, 17.f),
                    juce::Justification::centred);
        }
    }
}

}
