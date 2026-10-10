#pragma once

#include "Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

class NodeEditorPresentation;

class NodeEditorModelCommands final {
public:
    NodeEditorModelCommands(
            GraphDocument& document,
            GraphCommandDispatcher& commands,
            NodeEditorPresentation& presentation);

    bool begin(const String& nodeId, bool downstreamFeedback = true);
    bool publish(const String& nodeId, NodeModelStatePtr model);
    void finish();

private:
    GraphDocument& document;
    GraphCommandDispatcher& commands;
    NodeEditorPresentation& presentation;
    String activeNodeId;
    uint64_t durableBaseRevision {};
};

}
