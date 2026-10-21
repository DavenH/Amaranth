#include "UI/NodeCanvas.h"

#include "Graph/NodeDefinition.h"
#include "UI/NodeGroupPresentation.h"

namespace CycleV2 {

const Node* NodeCanvas::explanationNodeAt(Point<float> position) const {
    const auto& edges = queries.presentationFacts().edgeIndex();
    for (auto node = graph.getNodes().rbegin(); node != graph.getNodes().rend(); ++node) {
        if (graph.isNodeVisible(node->id) && ExplanationBadge::isVisible(*node, selectedNodeId)
                && ExplanationBadge::boundsFor(*node, viewport.toScreen(
                        NodeCanvasScene::presentationWorldBounds(graph, *node, edges)), viewport.getZoom(),
                        queries.presentationFacts().audioScopeAnalysis().scopeFor(node->id) == AuthoredAudioScope::Global).contains(position)) {
            return &*node;
        }
    }
    return nullptr;
}

const NodeGroup* NodeCanvas::explanationGroupAt(Point<float> position) const {
    const auto& edges = queries.presentationFacts().edgeIndex();
    for (auto group = graph.getNodeGroups().rbegin(); group != graph.getNodeGroups().rend(); ++group) {
        if (NodeGroupPresentation::informationBounds(graph, *group, viewport, edges).contains(position)) {
            return &*group;
        }
    }
    return nullptr;
}

bool NodeCanvas::finishExplanationEdit(const GraphEditResult& result) {
    if (!result.succeeded()) {
        return false;
    }
    if (result.changed) {
        editStatusMessage = "Explanation saved";
        notifyGraphDocumentStateChanged();
        requestCanvasRepaint();
    }
    return true;
}

bool NodeCanvas::setExplanation(const ExplanationTarget& target, const String& text) {
    return finishExplanationEdit(commands.setExplanationText(target, text));
}

bool NodeCanvas::setGroupInformationCard(const String& id, std::optional<Point<float>> offset) {
    const bool succeeded = finishExplanationEdit(commands.setGroupInformationCard(id, offset));
    if (succeeded && !offset.has_value() && focusedInformationGroup == id) {
        focusedInformationGroup = {};
        informationCardDrag.reset();
    }
    return succeeded;
}

bool NodeCanvas::setGroupInformationCardVisible(const String& id, bool visible) {
    const NodeGroup* group = graph.findNodeGroup(id);
    if (group == nullptr) {
        return false;
    }
    const auto offset = visible ? group->informationCardOffset.value_or(
            GroupInformationCardPresentation::defaultOffset(
                    graph, *group, queries.presentationFacts().edgeIndex())) : Point<float> {};
    return setGroupInformationCard(id, visible ? std::optional<Point<float>>(offset) : std::nullopt);
}

var NodeCanvas::inspectExplanation(const ExplanationTarget& target) const {
    const Node* node = target.scope == ExplanationScope::Node ? graph.findNode(target.id) : nullptr;
    const NodeGroup* group = target.scope == ExplanationScope::Group ? graph.findNodeGroup(target.id) : nullptr;
    if ((target.scope == ExplanationScope::Preset && target.id.isNotEmpty())
            || (target.scope == ExplanationScope::Node && node == nullptr)
            || (target.scope == ExplanationScope::Group && group == nullptr)) {
        return {};
    }
    auto* result = new DynamicObject();
    result->setProperty("scope", target.scope == ExplanationScope::Preset ? "preset"
            : target.scope == ExplanationScope::Node ? "node" : "group");
    if (target.scope != ExplanationScope::Preset) {
        result->setProperty(group != nullptr ? "groupId" : "nodeId", target.id);
    }
    result->setProperty("text", node != nullptr ? node->rationale
            : group != nullptr ? group->rationale : document.presentation().description);
    if (const auto* definition = node != nullptr
            ? NodeDefinitionRegistry::instance().find(node->kind) : nullptr) {
        result->setProperty("typeHelp", definition->helpText);
    }
    if (group != nullptr) {
        result->setProperty("cardEnabled", group->informationCardOffset.has_value());
        result->setProperty("cardVisible", group->informationCardOffset.has_value() && group->rationale.isNotEmpty());
        if (group->informationCardOffset.has_value()) {
            auto* offset = new DynamicObject();
            offset->setProperty("x", group->informationCardOffset->x);
            offset->setProperty("y", group->informationCardOffset->y);
            result->setProperty("cardOffset", offset);
        }
    }
    if (explanationEditor != nullptr && explanationEditorTarget == target) {
        result->setProperty("editor", explanationEditor->automationState());
    }
    return result;
}

bool NodeCanvas::openExplanationEditor(const ExplanationTarget& target) {
    const var current = inspectExplanation(target);
    if (current.isVoid()) {
        return false;
    }
    if (explanationEditor != nullptr) {
        explanationEditor->dismiss();
    }
    explanationEditorTarget = target;
    explanationEditorRevision = document.revision();
    const Node* node = target.scope == ExplanationScope::Node ? graph.findNode(target.id) : nullptr;
    const NodeGroup* group = target.scope == ExplanationScope::Group ? graph.findNodeGroup(target.id) : nullptr;
    const String title = node != nullptr ? labelForNodeKind(node->kind) + " explanation"
            : group != nullptr ? group->title + " explanation" : "Preset explanation";
    auto editor = std::make_unique<ExplanationEditor>(title,
            current.getProperty("typeHelp", {}).toString(), current["text"].toString(),
            [safeThis = SafePointer<NodeCanvas>(this), target, revision = explanationEditorRevision](const String& text) {
                return safeThis != nullptr && safeThis->document.revision() == revision
                        && safeThis->setExplanation(target, text);
            });
    explanationEditor = editor.get();
    const auto& edges = queries.presentationFacts().edgeIndex();
    const auto groupAnchor = group != nullptr
            ? NodeGroupPresentation::informationBounds(graph, *group, viewport, edges) : Rectangle<float>();
    const auto anchor = node != nullptr
            ? ExplanationBadge::boundsFor(*node, viewport.toScreen(NodeCanvasScene::presentationWorldBounds(
                    graph, *node, edges)), viewport.getZoom(),
                    queries.presentationFacts().audioScopeAnalysis().scopeFor(node->id) == AuthoredAudioScope::Global).toNearestInt()
            : group != nullptr ? (groupAnchor.isEmpty()
                    ? NodeGroupPresentation::labelBounds(graph, *group, viewport, edges) : groupAnchor).toNearestInt()
            : presetInformation.getBounds();
    auto& callout = CallOutBox::launchAsynchronously(std::move(editor), anchor, this);
    callout.updatePosition(anchor, canvasUtilityBounds().toNearestInt());
    callout.setDismissalMouseClicksAreAlwaysConsumed(true);
    explanationEditor->focusText();
    return true;
}

void NodeCanvas::updateInformationCards() const {
    informationCards.update(graph, queries.presentationFacts().edgeIndex(), document.revision());
}

std::optional<GroupInformationCardPreview> NodeCanvas::informationCardPreview() const {
    return informationCardDrag.has_value()
            ? std::optional<GroupInformationCardPreview>({ informationCardDrag->groupId, informationCardDrag->offset })
            : std::nullopt;
}

bool NodeCanvas::informationCardMouseDown(const MouseEvent& event) {
    updateInformationCards();
    const auto* card = informationCards.cardAt(event.position, viewport, informationCardPreview());
    focusedInformationGroup = card != nullptr ? card->groupId : String();
    informationCardDrag.reset();
    if (card == nullptr) {
        return false;
    }
    const auto bounds = GroupInformationCardPresentation::bounds(*card, viewport);
    if (event.mods.isPopupMenu()) {
        showNodeGroupMenu(card->groupId, event.position);
    } else if (bounds.close.contains(event.position)) {
        setGroupInformationCard(card->groupId, {});
    } else if (event.getNumberOfClicks() >= 2) {
        openExplanationEditor({ ExplanationScope::Group, card->groupId });
    } else if (bounds.header.contains(event.position)) {
        informationCardDrag = InformationCardDrag {
                card->groupId, viewport.toWorld(event.position), card->offset, card->offset, document.revision()
        };
    }
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::informationCardMouseDrag(const MouseEvent& event) {
    if (!informationCardDrag.has_value()) {
        return false;
    }
    informationCardDrag->offset = informationCardDrag->startOffset
            + viewport.toWorld(event.position) - informationCardDrag->startPointer;
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::informationCardMouseUp() {
    if (!informationCardDrag.has_value()) {
        return false;
    }
    const auto gesture = *informationCardDrag;
    informationCardDrag.reset();
    if (gesture.documentRevision == document.revision()) {
        setGroupInformationCard(gesture.groupId, gesture.offset);
    }
    requestCanvasRepaint();
    return true;
}

bool NodeCanvas::informationCardKey(const KeyPress& key) {
    if (key == KeyPress::escapeKey && (informationCardDrag.has_value() || focusedInformationGroup.isNotEmpty())) {
        informationCardDrag.reset();
        focusedInformationGroup = {};
        requestCanvasRepaint();
        return true;
    }
    if ((key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
            && focusedInformationGroup.isNotEmpty()) {
        return setGroupInformationCard(focusedInformationGroup, {});
    }
    return false;
}

}
