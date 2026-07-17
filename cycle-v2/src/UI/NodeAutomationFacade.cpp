#include "NodeAutomationFacade.h"

namespace CycleV2 {

NodeAutomationFacade::NodeAutomationFacade(GraphDocument& documentToUse,
        GraphCommandDispatcher& commandsToUse) :
        document(documentToUse), commands(commandsToUse) {
}

GraphEditResult NodeAutomationFacade::addNode(NodeKind kind, juce::Point<float> position) {
    return commands.addNode(kind, position);
}

GraphEditResult NodeAutomationFacade::moveNode(const juce::String& nodeId,
        juce::Point<float> position) {
    return commands.moveNode(nodeId, position);
}

GraphEditResult NodeAutomationFacade::connect(const PortAddress& source,
        const PortAddress& destination) {
    return commands.connect(source, destination);
}

GraphEditResult NodeAutomationFacade::deleteNode(const juce::String& nodeId) {
    return commands.removeNode(nodeId);
}

GraphEditResult NodeAutomationFacade::deleteEdge(int edgeIndex) {
    return edgeIndex < 0
            ? GraphEditResult { GraphEditCode::MissingEdge }
            : commands.removeEdgeAt((size_t) edgeIndex);
}

GraphEditResult NodeAutomationFacade::setParameter(const juce::String& nodeId,
        const juce::String& parameterId, const juce::String& label,
        const juce::String& value) {
    return commands.setNodeParameter(nodeId, parameterId, label, value);
}

bool NodeAutomationFacade::getParameter(const juce::String& nodeId,
        const juce::String& parameterId, juce::String& value) const {
    const Node* node = document.graph().findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    for (const auto& parameter : node->parameters) {
        if (parameter.id == parameterId) {
            value = parameter.value;
            return true;
        }
    }
    return false;
}

}
