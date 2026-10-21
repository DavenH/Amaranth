#pragma once

#include <map>
#include <optional>

#include "Graph/NodeGraph.h"
#include "UI/NodeCanvasViewport.h"

namespace CycleV2 {

class GraphEdgeIndex;

struct GroupInformationCardPreview {
    juce::String groupId;
    juce::Point<float> offset;
};

struct GroupInformationCardBounds {
    juce::Rectangle<float> card;
    juce::Rectangle<float> header;
    juce::Rectangle<float> body;
    juce::Rectangle<float> close;
};

struct GroupInformationCard {
    juce::String groupId;
    juce::String title;
    juce::String text;
    juce::Point<float> origin;
    juce::Point<float> offset;
    juce::TextLayout layout;
    float height {};
};

class GroupInformationCardPresentation {
public:
    void update(const NodeGraph& graph, const GraphEdgeIndex& edges, uint64_t documentRevision);
    const std::vector<const GroupInformationCard*>& visibleCards() const { return visible; }
    const GroupInformationCard* find(const juce::String& groupId) const;
    const GroupInformationCard* cardAt(juce::Point<float> position,
            const NodeCanvasViewport& viewport,
            const std::optional<GroupInformationCardPreview>& preview) const;
    void paint(juce::Graphics& graphics, const NodeCanvasViewport& viewport,
            const juce::String& focusedGroup,
            const std::optional<GroupInformationCardPreview>& preview) const;

    static GroupInformationCardBounds bounds(const GroupInformationCard& card,
            const NodeCanvasViewport& viewport,
            const std::optional<GroupInformationCardPreview>& preview = {});
    static juce::Point<float> defaultOffset(const NodeGraph& graph,
            const NodeGroup& group, const GraphEdgeIndex& edges);

private:
    std::map<juce::String, GroupInformationCard> cards;
    std::vector<const GroupInformationCard*> visible;
    const NodeGraph* cachedGraph {};
    uint64_t graphRevision {};
    uint64_t documentRevision {};
};

}
