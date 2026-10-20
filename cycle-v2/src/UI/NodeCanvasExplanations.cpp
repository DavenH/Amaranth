#include "UI/NodeCanvas.h"

#include "Graph/NodeDefinition.h"

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

bool NodeCanvas::setExplanation(const String& nodeId, const String& text) {
    const auto result = nodeId.isEmpty() ? commands.setPresetDescription(text)
            : commands.setNodeRationale(nodeId, text);
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

var NodeCanvas::inspectExplanation(const String& nodeId) const {
    const Node* node = nodeId.isEmpty() ? nullptr : graph.findNode(nodeId);
    if (nodeId.isNotEmpty() && node == nullptr) {
        return {};
    }
    auto* result = new DynamicObject();
    result->setProperty("scope", nodeId.isEmpty() ? "preset" : "node");
    result->setProperty("nodeId", nodeId);
    result->setProperty("text", node != nullptr ? node->rationale : document.presentation().description);
    if (const auto* definition = node != nullptr
            ? NodeDefinitionRegistry::instance().find(node->kind) : nullptr) {
        result->setProperty("typeHelp", definition->helpText);
    }
    if (explanationEditor != nullptr && explanationEditorNodeId == nodeId) {
        result->setProperty("editor", explanationEditor->automationState());
    }
    return result;
}

bool NodeCanvas::openExplanationEditor(const String& nodeId) {
    const Node* node = nodeId.isEmpty() ? nullptr : graph.findNode(nodeId);
    if (nodeId.isNotEmpty() && node == nullptr) {
        return false;
    }
    if (explanationEditor != nullptr) {
        explanationEditor->dismiss();
    }
    explanationEditorNodeId = nodeId;
    const auto* definition = node != nullptr
            ? NodeDefinitionRegistry::instance().find(node->kind) : nullptr;
    const String title = node != nullptr ? labelForNodeKind(node->kind) + " explanation"
            : "Preset explanation";
    auto editor = std::make_unique<ExplanationEditor>(title,
            definition != nullptr ? definition->helpText : String(),
            node != nullptr ? node->rationale : document.presentation().description,
            [safeThis = SafePointer<NodeCanvas>(this), nodeId](const String& text) {
                return safeThis != nullptr && safeThis->setExplanation(nodeId, text);
            });
    explanationEditor = editor.get();
    const auto anchor = node != nullptr
            ? ExplanationBadge::boundsFor(*node, viewport.toScreen(NodeCanvasScene::presentationWorldBounds(
                    graph, *node, queries.presentationFacts().edgeIndex())), viewport.getZoom(),
                    queries.presentationFacts().audioScopeAnalysis().scopeFor(node->id) == AuthoredAudioScope::Global).toNearestInt()
            : presetInformation.getBounds();
    auto& callout = CallOutBox::launchAsynchronously(std::move(editor), anchor, this);
    callout.updatePosition(anchor, canvasUtilityBounds().toNearestInt());
    callout.setDismissalMouseClicksAreAlwaysConsumed(true);
    explanationEditor->focusText();
    return true;
}

}
