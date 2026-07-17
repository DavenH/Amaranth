#pragma once

#include "../Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

class NodeAutomationFacade {
public:
    NodeAutomationFacade(GraphDocument& document, GraphCommandDispatcher& commands);
    GraphEditResult addNode(NodeKind kind, juce::Point<float> position);
    GraphEditResult moveNode(const juce::String& nodeId, juce::Point<float> position);
    GraphEditResult connect(const PortAddress& source, const PortAddress& destination);
    GraphEditResult deleteNode(const juce::String& nodeId);
    GraphEditResult deleteEdge(int edgeIndex);
    GraphEditResult setParameter(const juce::String& nodeId, const juce::String& parameterId,
            const juce::String& label, const juce::String& value);
    bool getParameter(const juce::String& nodeId, const juce::String& parameterId,
            juce::String& value) const;
private:
    GraphDocument& document;
    GraphCommandDispatcher& commands;
};

}
